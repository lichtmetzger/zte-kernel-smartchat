/*
 * drivers/gpu/ion/ion.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/ion.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#include <mach/iommu_domains.h>
#include "ion_priv.h"
#define DEBUG

/**
 * struct ion_device - the metadata of the ion device node
 * @dev:		the actual misc device
 * @buffers:	an rb tree of all the existing buffers
 * @lock:		lock protecting the buffers & heaps trees
 * @heaps:		list of all the heaps in the system
 * @user_clients:	list of all the clients created from userspace
 */
struct ion_device {
	struct miscdevice dev;
	struct rb_root buffers;
	struct mutex lock;
	struct rb_root heaps;
	long (*custom_ioctl) (struct ion_client *client, unsigned int cmd,
			      unsigned long arg);
	struct rb_root user_clients;
	struct rb_root kernel_clients;
	struct dentry *debug_root;
};

/**
 * struct ion_client - a process/hw block local address space
 * @ref:		for reference counting the client
 * @node:		node in the tree of all clients
 * @dev:		backpointer to ion device
 * @handles:		an rb tree of all the handles in this client
 * @lock:		lock protecting the tree of handles
 * @heap_mask:		mask of all supported heaps
 * @name:		used for debugging
 * @task:		used for debugging
 *
 * A client represents a list of buffers this client may access.
 * The mutex stored here is used to protect both handles tree
 * as well as the handles themselves, and should be held while modifying either.
 */
struct ion_client {
	struct kref ref;
	struct rb_node node;
	struct ion_device *dev;
	struct rb_root handles;
	struct mutex lock;
	unsigned int heap_mask;
	const char *name;
	struct task_struct *task;
	pid_t pid;
	struct dentry *debug_root;
};

/**
 * ion_handle - a client local reference to a buffer
 * @ref:		reference count
 * @client:		back pointer to the client the buffer resides in
 * @buffer:		pointer to the buffer
 * @node:		node in the client's handle rbtree
 * @kmap_cnt:		count of times this client has mapped to kernel
 * @dmap_cnt:		count of times this client has mapped for dma
 * @usermap_cnt:	count of times this client has mapped for userspace
 *
 * Modifications to node, map_cnt or mapping should be protected by the
 * lock in the client.  Other fields are never changed after initialization.
 */
struct ion_handle {
	struct kref ref;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct rb_node node;
	unsigned int kmap_cnt;
	unsigned int dmap_cnt;
	unsigned int usermap_cnt;
	unsigned int iommu_map_cnt;
};

static int ion_validate_buffer_flags(struct ion_buffer *buffer,
					unsigned long flags)
{
	if (buffer->kmap_cnt || buffer->dmap_cnt || buffer->umap_cnt ||
		buffer->iommu_map_cnt) {
		if (buffer->flags != flags) {
			pr_err("%s: buffer was already mapped with flags %lx,"
				" cannot map with flags %lx\n", __func__,
				buffer->flags, flags);
			return 1;
		}

	} else {
		buffer->flags = flags;
	}
	return 0;
}

/* this function should only be called while dev->lock is held */
static void ion_buffer_add(struct ion_device *dev,
			   struct ion_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct ion_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_buffer, node);

		if (buffer < entry) {
			p = &(*p)->rb_left;
		} else if (buffer > entry) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: buffer already found.", __func__);
			BUG();
		}
	}

	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &dev->buffers);
}

static void ion_iommu_add(struct ion_buffer *buffer,
			  struct ion_iommu_map *iommu)
{
	struct rb_node **p = &buffer->iommu_maps.rb_node;
	struct rb_node *parent = NULL;
	struct ion_iommu_map *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_iommu_map, node);

		if (iommu->key < entry->key) {
			p = &(*p)->rb_left;
		} else if (iommu->key > entry->key) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: buffer %p already has mapping for domain %d"
				" and partition %d\n", __func__,
				buffer,
				iommu_map_domain(iommu),
				iommu_map_partition(iommu));
			BUG();
		}
	}

	rb_link_node(&iommu->node, parent, p);
	rb_insert_color(&iommu->node, &buffer->iommu_maps);

}

static struct ion_iommu_map *ion_iommu_lookup(struct ion_buffer *buffer,
						unsigned int domain_no,
						unsigned int partition_no)
{
	struct rb_node **p = &buffer->iommu_maps.rb_node;
	struct rb_node *parent = NULL;
	struct ion_iommu_map *entry;
	uint64_t key = domain_no;
	key = key << 32 | partition_no;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_iommu_map, node);

		if (key < entry->key)
			p = &(*p)->rb_left;
		else if (key > entry->key)
			p = &(*p)->rb_right;
		else
			return entry;
	}

	return NULL;
}

/* this function should only be called while dev->lock is held */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
				     struct ion_device *dev,
				     unsigned long len,
				     unsigned long align,
				     unsigned long flags)
{
	struct ion_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(struct ion_buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->heap = heap;
	kref_init(&buffer->ref);

	ret = heap->ops->allocate(heap, buffer, len, align, flags);
	if (ret) {
		kfree(buffer);
		return ERR_PTR(ret);
	}
	buffer->dev = dev;
	buffer->size = len;
	mutex_init(&buffer->lock);
	ion_buffer_add(dev, buffer);
	return buffer;
}

static void ion_buffer_destroy(struct kref *kref)
{
	struct ion_buffer *buffer = container_of(kref, struct ion_buffer, ref);
	struct ion_device *dev = buffer->dev;

	buffer->heap->ops->free(buffer);
	mutex_lock(&dev->lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->lock);
	kfree(buffer);
}

static void ion_buffer_get(struct ion_buffer *buffer)
{
	kref_get(&buffer->ref);
}

static int ion_buffer_put(struct ion_buffer *buffer)
{
	return kref_put(&buffer->ref, ion_buffer_destroy);
}

static struct ion_handle *ion_handle_create(struct ion_client *client,
				     struct ion_buffer *buffer)
{
	struct ion_handle *handle;

	handle = kzalloc(sizeof(struct ion_handle), GFP_KERNEL);
	if (!handle)
		return ERR_PTR(-ENOMEM);
	kref_init(&handle->ref);
	rb_init_node(&handle->node);
	handle->client = client;
	ion_buffer_get(buffer);
	handle->buffer = buffer;

	return handle;
}

static void ion_handle_destroy(struct kref *kref)
{
	struct ion_handle *handle = container_of(kref, struct ion_handle, ref);
	/* XXX Can a handle be destroyed while it's map count is non-zero?:
	   if (handle->map_cnt) unmap
	 */
	WARN_ON(handle->kmap_cnt || handle->dmap_cnt || handle->usermap_cnt);
	ion_buffer_put(handle->buffer);
	mutex_lock(&handle->client->lock);
	if (!RB_EMPTY_NODE(&handle->node))
		rb_erase(&handle->node, &handle->client->handles);
	mutex_unlock(&handle->client->lock);
	kfree(handle);
}

struct ion_buffer *ion_handle_buffer(struct ion_handle *handle)
{
	return handle->buffer;
}

static void ion_handle_get(struct ion_handle *handle)
{
	kref_get(&handle->ref);
}

static int ion_handle_put(struct ion_handle *handle)
{
	return kref_put(&handle->ref, ion_handle_destroy);
}

static struct ion_handle *ion_handle_lookup(struct ion_client *client,
					    struct ion_buffer *buffer)
{
	struct rb_node *n;

	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);
		if (handle->buffer == buffer)
			return handle;
	}
	return NULL;
}

static bool ion_handle_validate(struct ion_client *client, struct ion_handle *handle)
{
	struct rb_node *n = client->handles.rb_node;

	while (n) {
		struct ion_handle *handle_node = rb_entry(n, struct ion_handle,
							  node);
		if (handle < handle_node)
			n = n->rb_left;
		else if (handle > handle_node)
			n = n->rb_right;
		else
			return true;
	}
	return false;
}

static void ion_handle_add(struct ion_client *client, struct ion_handle *handle)
{
	struct rb_node **p = &client->handles.rb_node;
	struct rb_node *parent = NULL;
	struct ion_handle *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_handle, node);

		if (handle < entry)
			p = &(*p)->rb_left;
		else if (handle > entry)
			p = &(*p)->rb_right;
		else
			WARN(1, "%s: buffer already found.", __func__);
	}

	rb_link_node(&handle->node, parent, p);
	rb_insert_color(&handle->node, &client->handles);
}

struct ion_handle *ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int flags)
{
	struct rb_node *n;
	struct ion_handle *handle;
	struct ion_device *dev = client->dev;
	struct ion_buffer *buffer = NULL;
	unsigned long secure_allocation = flags & ION_SECURE;

	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	mutex_lock(&dev->lock);
	for (n = rb_first(&dev->heaps); n != NULL; n = rb_next(n)) {
		struct ion_heap *heap = rb_entry(n, struct ion_heap, node);
		/* if the client doesn't support this heap type */
		if (!((1 << heap->type) & client->heap_mask))
			continue;
		/* if the caller didn't specify this heap type */
		if (!((1 << heap->id) & flags))
			continue;
		/* Do not allow un-secure heap if secure is specified */
		if (secure_allocation && (heap->type != ION_HEAP_TYPE_CP))
			continue;
		buffer = ion_buffer_create(heap, dev, len, align, flags);
		if (!IS_ERR_OR_NULL(buffer))
			break;
	}
	mutex_unlock(&dev->lock);

	if (IS_ERR_OR_NULL(buffer))
		return ERR_PTR(PTR_ERR(buffer));

	handle = ion_handle_create(client, buffer);

	if (IS_ERR_OR_NULL(handle))
		goto end;

	/*
	 * ion_buffer_create will create a buffer with a ref_cnt of 1,
	 * and ion_handle_create will take a second reference, drop one here
	 */
	ion_buffer_put(buffer);

	mutex_lock(&client->lock);
	ion_handle_add(client, handle);
	mutex_unlock(&client->lock);
	return handle;

end:
	ion_buffer_put(buffer);
	return handle;
}
EXPORT_SYMBOL(ion_alloc);

void ion_free(struct ion_client *client, struct ion_handle *handle)
{
	bool valid_handle;

	BUG_ON(client != handle->client);

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, handle);
	mutex_unlock(&client->lock);

	if (!valid_handle) {
		WARN("%s: invalid handle passed to free.\n", __func__);
		return;
	}
	ion_handle_put(handle);
}
EXPORT_SYMBOL(ion_free);

static void ion_client_get(struct ion_client *client);
static int ion_client_put(struct ion_client *client);

static bool _ion_map(int *buffer_cnt, int *handle_cnt)
{
	bool map;

	BUG_ON(*handle_cnt != 0 && *buffer_cnt == 0);

	if (*buffer_cnt)
		map = false;
	else
		map = true;
	if (*handle_cnt == 0)
		(*buffer_cnt)++;
	(*handle_cnt)++;
	return map;
}

static bool _ion_unmap(int *buffer_cnt, int *handle_cnt)
{
	BUG_ON(*handle_cnt == 0);
	(*handle_cnt)--;
	if (*handle_cnt != 0)
		return false;
	BUG_ON(*buffer_cnt == 0);
	(*buffer_cnt)--;
	if (*buffer_cnt == 0)
		return true;
	return false;
}

int ion_phys(struct ion_client *client, struct ion_handle *handle,
	     ion_phys_addr_t *addr, size_t *len)
{
	struct ion_buffer *buffer;
	int ret;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		mutex_unlock(&client->lock);
		return -EINVAL;
	}

	buffer = handle->buffer;

	if (!buffer->heap->ops->phys) {
		pr_err("%s: ion_phys is not implemented by this heap.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return -ENODEV;
	}
	mutex_unlock(&client->lock);
	ret = buffer->heap->ops->phys(buffer->heap, buffer, addr, len);
	return ret;
}
EXPORT_SYMBOL(ion_phys);

void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle,
			unsigned long flags)
{
	struct ion_buffer *buffer;
	void *vaddr;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_kernel.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}

	buffer = handle->buffer;
	mutex_lock(&buffer->lock);

	if (!handle->buffer->heap->ops->map_kernel) {
		pr_err("%s: map_kernel is not implemented by this heap.\n",
		       __func__);
		mutex_unlock(&buffer->lock);
		mutex_unlock(&client->lock);
		return ERR_PTR(-ENODEV);
	}

	if (ion_validate_buffer_flags(buffer, flags)) {
			vaddr = ERR_PTR(-EEXIST);
			goto out;
	}

	if (_ion_map(&buffer->kmap_cnt, &handle->kmap_cnt)) {
		vaddr = buffer->heap->ops->map_kernel(buffer->heap, buffer,
							flags);
		if (IS_ERR_OR_NULL(vaddr))
			_ion_unmap(&buffer->kmap_cnt, &handle->kmap_cnt);
		buffer->vaddr = vaddr;
	} else {
		vaddr = buffer->vaddr;
	}

out:
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
	return vaddr;
}
EXPORT_SYMBOL(ion_map_kernel);

static int __ion_iommu_map(struct ion_buffer *buffer,
		int domain_num, int partition_num, unsigned long align,
		unsigned long iova_length, unsigned long flags,
		unsigned long *iova)
{
	struct ion_iommu_map *data;
	int ret;

	data = kmalloc(sizeof(*data), GFP_ATOMIC);

	if (!data)
		return -ENOMEM;

	data->buffer = buffer;
	iommu_map_domain(data) = domain_num;
	iommu_map_partition(data) = partition_num;

	ret = buffer->heap->ops->map_iommu(buffer, data,
						domain_num,
						partition_num,
						align,
						iova_length,
						flags);

	if (ret)
		goto out;

	kref_init(&data->ref);
	*iova = data->iova_addr;

	ion_iommu_add(buffer, data);

	return 0;

out:
	msm_free_iova_address(data->iova_addr, domain_num, partition_num,
				buffer->size);
	kfree(data);
	return ret;
}

int ion_map_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num, unsigned long align,
			unsigned long iova_length, unsigned long *iova,
			unsigned long *buffer_size,
			unsigned long flags)
{
	struct ion_buffer *buffer;
	struct ion_iommu_map *iommu_map;
	int ret = 0;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_kernel.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}

	buffer = handle->buffer;
	mutex_lock(&buffer->lock);

	if (!handle->buffer->heap->ops->map_iommu) {
		pr_err("%s: map_iommu is not implemented by this heap.\n",
		       __func__);
		ret = -ENODEV;
		goto out;
	}

	if (ion_validate_buffer_flags(buffer, flags)) {
		ret = -EEXIST;
		goto out;
	}

	/*
	 * If clients don't want a custom iova length, just use whatever
	 * the buffer size is
	 */
	if (!iova_length)
		iova_length = buffer->size;

	if (buffer->size > iova_length) {
		pr_debug("%s: iova length %lx is not at least buffer size"
			" %x\n", __func__, iova_length, buffer->size);
		ret = -EINVAL;
		goto out;
	}

	if (buffer->size & ~PAGE_MASK) {
		pr_debug("%s: buffer size %x is not aligned to %lx", __func__,
			buffer->size, PAGE_SIZE);
		ret = -EINVAL;
		goto out;
	}

	if (iova_length & ~PAGE_MASK) {
		pr_debug("%s: iova_length %lx is not aligned to %lx", __func__,
			iova_length, PAGE_SIZE);
		ret = -EINVAL;
		goto out;
	}

	iommu_map = ion_iommu_lookup(buffer, domain_num, partition_num);
	if (_ion_map(&buffer->iommu_map_cnt, &handle->iommu_map_cnt) ||
		!iommu_map) {
		ret = __ion_iommu_map(buffer, domain_num, partition_num, align,
					iova_length, flags, iova);
		if (ret < 0)
			_ion_unmap(&buffer->iommu_map_cnt,
				   &handle->iommu_map_cnt);
	} else {
		if (iommu_map->mapped_size != iova_length) {
			pr_err("%s: handle %p is already mapped with length"
				" %x, trying to map with length %lx\n",
				__func__, handle, iommu_map->mapped_size,
				iova_length);
			_ion_unmap(&buffer->iommu_map_cnt,
				   &handle->iommu_map_cnt);
			ret = -EINVAL;
		} else {
			kref_get(&iommu_map->ref);
			*iova = iommu_map->iova_addr;
		}
	}
	*buffer_size = buffer->size;
out:
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
	return ret;
}
EXPORT_SYMBOL(ion_map_iommu);

static void ion_iommu_release(struct kref *kref)
{
	struct ion_iommu_map *map = container_of(kref, struct ion_iommu_map,
						ref);
	struct ion_buffer *buffer = map->buffer;

	rb_erase(&map->node, &buffer->iommu_maps);
	buffer->heap->ops->unmap_iommu(map);
	kfree(map);
}

void ion_unmap_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num)
{
	struct ion_iommu_map *iommu_map;
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	buffer = handle->buffer;

	mutex_lock(&buffer->lock);

	iommu_map = ion_iommu_lookup(buffer, domain_num, partition_num);

	if (!iommu_map) {
		WARN(1, "%s: (%d,%d) was never mapped for %p\n", __func__,
				domain_num, partition_num, buffer);
		goto out;
	}

	_ion_unmap(&buffer->iommu_map_cnt, &handle->iommu_map_cnt);
	kref_put(&iommu_map->ref, ion_iommu_release);

out:
	mutex_unlock(&buffer->lock);

	mutex_unlock(&client->lock);

}
EXPORT_SYMBOL(ion_unmap_iommu);

struct scatterlist *ion_map_dma(struct ion_client *client,
				struct ion_handle *handle,
				unsigned long flags)
{
	struct ion_buffer *buffer;
	struct scatterlist *sglist;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_dma.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);

	if (!handle->buffer->heap->ops->map_dma) {
		pr_err("%s: map_kernel is not implemented by this heap.\n",
		       __func__);
		mutex_unlock(&buffer->lock);
		mutex_unlock(&client->lock);
		return ERR_PTR(-ENODEV);
	}

	if (ion_validate_buffer_flags(buffer, flags)) {
		sglist = ERR_PTR(-EEXIST);
		goto out;
	}

	if (_ion_map(&buffer->dmap_cnt, &handle->dmap_cnt)) {
		sglist = buffer->heap->ops->map_dma(buffer->heap, buffer);
		if (IS_ERR_OR_NULL(sglist))
			_ion_unmap(&buffer->dmap_cnt, &handle->dmap_cnt);
		buffer->sglist = sglist;
	} else {
		sglist = buffer->sglist;
	}

out:
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
	return sglist;
}
EXPORT_SYMBOL(ion_map_dma);

void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);
	if (_ion_unmap(&buffer->kmap_cnt, &handle->kmap_cnt)) {
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
}
EXPORT_SYMBOL(ion_unmap_kernel);

void ion_unmap_dma(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);
	if (_ion_unmap(&buffer->dmap_cnt, &handle->dmap_cnt)) {
		buffer->heap->ops->unmap_dma(buffer->heap, buffer);
		buffer->sglist = NULL;
	}
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
}
EXPORT_SYMBOL(ion_unmap_dma);

struct ion_buffer *ion_share(struct ion_client *client,
				 struct ion_handle *handle)
{
	bool valid_handle;

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, handle);
	mutex_unlock(&client->lock);
	if (!valid_handle) {
		WARN("%s: invalid handle passed to share.\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	/* do not take an extra reference here, the burden is on the caller
	 * to make sure the buffer doesn't go away while it's passing it
	 * to another client -- ion_free should not be called on this handle
	 * until the buffer has been imported into the other client
	 */
	return handle->buffer;
}
EXPORT_SYMBOL(ion_share);

struct ion_handle *ion_import(struct ion_client *client,
			      struct ion_buffer *buffer)
{
	struct ion_handle *handle = NULL;

	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (!IS_ERR_OR_NULL(handle)) {
		ion_handle_get(handle);
		goto end;
	}
	handle = ion_handle_create(client, buffer);
	if (IS_ERR_OR_NULL(handle))
		goto end;
	ion_handle_add(client, handle);
end:
	mutex_unlock(&client->lock);
	return handle;
}
EXPORT_SYMBOL(ion_import);

static int check_vaddr_bounds(unsigned long start, unsigned long end)
{
	struct mm_struct *mm = current->active_mm;
	struct vm_area_struct *vma;
	int ret = 1;

	if (end < start)
		goto out;

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, start);
	if (vma && vma->vm_start < end) {
		if (start < vma->vm_start)
			goto out_up;
		if (end > vma->vm_end)
			goto out_up;
		ret = 0;
	}

out_up:
	up_read(&mm->mmap_sem);
out:
	return ret;
}

static int ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *uaddr, unsigned long offset, unsigned long len,
			unsigned int cmd)
{
	struct ion_buffer *buffer;
	int ret = -EINVAL;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to do_cache_op.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);

	if (!ION_IS_CACHED(buffer->flags)) {
		ret = 0;
		goto out;
	}

	if (!handle->buffer->heap->ops->cache_op) {
		pr_err("%s: cache_op is not implemented by this heap.\n",
		       __func__);
		ret = -ENODEV;
		goto out;
	}


	ret = buffer->heap->ops->cache_op(buffer->heap, buffer, uaddr,
						offset, len, cmd);

out:
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
	return ret;

}

static const struct file_operations ion_share_fops;

struct ion_handle *ion_import_fd(struct ion_client *client, int fd)
{
	struct file *file = fget(fd);
	struct ion_handle *handle;

	if (!file) {
		pr_err("%s: imported fd not found in file table.\n", __func__);
		return ERR_PTR(-EINVAL);
	}
	if (file->f_op != &ion_share_fops) {
		pr_err("%s: imported file %s is not a shared ion"
			" file.", __func__, file->f_dentry->d_name.name);
		handle = ERR_PTR(-EINVAL);
		goto end;
	}
	handle = ion_import(client, file->private_data);
end:
	fput(file);
	return handle;
}
EXPORT_SYMBOL(ion_import_fd);

static int ion_debug_client_show(struct seq_file *s, void *unused)
{
	struct ion_client *client = s->private;
	struct rb_node *n;

	seq_printf(s, "%16.16s: %16.16s : %16.16s : %16.16s\n", "heap_name",
			"size_in_bytes", "handle refcount", "buffer");
	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);

		seq_printf(s, "%16.16s: %16x : %16d : %16p\n",
				handle->buffer->heap->name,
				handle->buffer->size,
				atomic_read(&handle->ref.refcount),
				handle->buffer);
	}

	seq_printf(s, "%16.16s %d\n", "client refcount:",
			atomic_read(&client->ref.refcount));
	mutex_unlock(&client->lock);

	return 0;
}

static int ion_debug_client_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_client_show, inode->i_private);
}

static const struct file_operations debug_client_fops = {
	.open = ion_debug_client_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct ion_client *ion_client_lookup(struct ion_device *dev,
					    struct task_struct *task)
{
	struct rb_node *n = dev->user_clients.rb_node;
	struct ion_client *client;

	mutex_lock(&dev->lock);
	while (n) {
		client = rb_entry(n, struct ion_client, node);
		if (task == client->task) {
			ion_client_get(client);
			mutex_unlock(&dev->lock);
			return client;
		} else if (task < client->task) {
			n = n->rb_left;
		} else if (task > client->task) {
			n = n->rb_right;
		}
	}
	mutex_unlock(&dev->lock);
	return NULL;
}

struct ion_client *ion_client_create(struct ion_device *dev,
				     unsigned int heap_mask,
				     const char *name)
{
	struct ion_client *client;
	struct task_struct *task;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct ion_client *entry;
	pid_t pid;

	get_task_struct(current->group_leader);
	task_lock(current->group_leader);
	pid = task_pid_nr(current->group_leader);
	/* don't bother to store task struct for kernel threads,
	   they can't be killed anyway */
	if (current->group_leader->flags & PF_KTHREAD) {
		put_task_struct(current->group_leader);
		task = NULL;
	} else {
		task = current->group_leader;
	}
	task_unlock(current->group_leader);

	/* if this isn't a kernel thread, see if a client already
	   exists */
	if (task) {
		client = ion_client_lookup(dev, task);
		if (!IS_ERR_OR_NULL(client)) {
			put_task_struct(current->group_leader);
			return client;
		}
	}

	client = kzalloc(sizeof(struct ion_client), GFP_KERNEL);
	if (!client) {
		put_task_struct(current->group_leader);
		return ERR_PTR(-ENOMEM);
	}

	client->dev = dev;
	client->handles = RB_ROOT;
	mutex_init(&client->lock);
	client->name = name;
	client->heap_mask = heap_mask;
	client->task = task;
	client->pid = pid;
	kref_init(&client->ref);

	mutex_lock(&dev->lock);
	if (task) {
		p = &dev->user_clients.rb_node;
		while (*p) {
			parent = *p;
			entry = rb_entry(parent, struct ion_client, node);

			if (task < entry->task)
				p = &(*p)->rb_left;
			else if (task > entry->task)
				p = &(*p)->rb_right;
		}
		rb_link_node(&client->node, parent, p);
		rb_insert_color(&client->node, &dev->user_clients);
	} else {
		p = &dev->kernel_clients.rb_node;
		while (*p) {
			parent = *p;
			entry = rb_entry(parent, struct ion_client, node);

			if (client < entry)
				p = &(*p)->rb_left;
			else if (client > entry)
				p = &(*p)->rb_right;
		}
		rb_link_node(&client->node, parent, p);
		rb_insert_color(&client->node, &dev->kernel_clients);
	}


	client->debug_root = debugfs_create_file(name, 0664,
						 dev->debug_root, client,
						 &debug_client_fops);
	mutex_unlock(&dev->lock);

	return client;
}

static void _ion_client_destroy(struct kref *kref)
{
	struct ion_client *client = container_of(kref, struct ion_client, ref);
	struct ion_device *dev = client->dev;
	struct rb_node *n;

	pr_debug("%s: %d\n", __func__, __LINE__);
	while ((n = rb_first(&client->handles))) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);
		ion_handle_destroy(&handle->ref);
	}
	mutex_lock(&dev->lock);
	if (client->task) {
		rb_erase(&client->node, &dev->user_clients);
		put_task_struct(client->task);
	} else {
		rb_erase(&client->node, &dev->kernel_clients);
	}
	debugfs_remove_recursive(client->debug_root);
	mutex_unlock(&dev->lock);

	kfree(client);
}

static void ion_client_get(struct ion_client *client)
{
	kref_get(&client->ref);
}

static int ion_client_put(struct ion_client *client)
{
	return kref_put(&client->ref, _ion_client_destroy);
}

void ion_client_destroy(struct ion_client *client)
{
	if (client)
		ion_client_put(client);
}
EXPORT_SYMBOL(ion_client_destroy);

int ion_handle_get_flags(struct ion_client *client, struct ion_handle *handle,
			unsigned long *flags)
{
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to %s.\n",
		       __func__, __func__);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);
	*flags = buffer->flags;
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);

	return 0;
}
EXPORT_SYMBOL(ion_handle_get_flags);

int ion_handle_get_size(struct ion_client *client, struct ion_handle *handle,
			unsigned long *size)
{
	struct ion_buffer *buffer;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to %s.\n",
		       __func__, __func__);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);
	*size = buffer->size;
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);

	return 0;
}
EXPORT_SYMBOL(ion_handle_get_size);

static int ion_share_release(struct inode *inode, struct file* file)
{
	struct ion_buffer *buffer = file->private_data;

	pr_debug("%s: %d\n", __func__, __LINE__);
	/* drop the reference to the buffer -- this prevents the
	   buffer from going away because the client holding it exited
	   while it was being passed */
	ion_buffer_put(buffer);
	return 0;
}

static void ion_vma_open(struct vm_area_struct *vma)
{

	struct ion_buffer *buffer = vma->vm_file->private_data;
	struct ion_handle *handle = vma->vm_private_data;
	struct ion_client *client;

	pr_debug("%s: %d\n", __func__, __LINE__);
	/* check that the client still exists and take a reference so
	   it can't go away until this vma is closed */
	client = ion_client_lookup(buffer->dev, current->group_leader);
	if (IS_ERR_OR_NULL(client)) {
		vma->vm_private_data = NULL;
		return;
	}
	ion_handle_get(handle);
	mutex_lock(&buffer->lock);
	buffer->umap_cnt++;
	mutex_unlock(&buffer->lock);
	pr_debug("%s: %d client_cnt %d handle_cnt %d alloc_cnt %d\n",
		 __func__, __LINE__,
		 atomic_read(&client->ref.refcount),
		 atomic_read(&handle->ref.refcount),
		 atomic_read(&buffer->ref.refcount));
}

static void ion_vma_close(struct vm_area_struct *vma)
{
	struct ion_handle *handle = vma->vm_private_data;
	struct ion_buffer *buffer = vma->vm_file->private_data;
	struct ion_client *client;

	pr_debug("%s: %d\n", __func__, __LINE__);
	/* this indicates the client is gone, nothing to do here */
	if (!handle)
		return;
	client = handle->client;
	mutex_lock(&buffer->lock);
	buffer->umap_cnt--;
	mutex_unlock(&buffer->lock);

	if (buffer->heap->ops->unmap_user)
		buffer->heap->ops->unmap_user(buffer->heap, buffer);


	pr_debug("%s: %d client_cnt %d handle_cnt %d alloc_cnt %d\n",
		 __func__, __LINE__,
		 atomic_read(&client->ref.refcount),
		 atomic_read(&handle->ref.refcount),
		 atomic_read(&buffer->ref.refcount));
	ion_handle_put(handle);
	ion_client_put(client);
	pr_debug("%s: %d client_cnt %d handle_cnt %d alloc_cnt %d\n",
		 __func__, __LINE__,
		 atomic_read(&client->ref.refcount),
		 atomic_read(&handle->ref.refcount),
		 atomic_read(&buffer->ref.refcount));
}

static struct vm_operations_struct ion_vm_ops = {
	.open = ion_vma_open,
	.close = ion_vma_close,
};

static int ion_share_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = file->private_data;
	unsigned long size = vma->vm_end - vma->vm_start;
	struct ion_client *client;
	struct ion_handle *handle;
	int ret;
	unsigned long flags = file->f_flags & O_DSYNC ?
				ION_SET_CACHE(UNCACHED) :
				ION_SET_CACHE(CACHED);


	pr_debug("%s: %d\n", __func__, __LINE__);
	/* make sure the client still exists, it's possible for the client to
	   have gone away but the map/share fd still to be around, take
	   a reference to it so it can't go away while this mapping exists */
	client = ion_client_lookup(buffer->dev, current->group_leader);
	if (IS_ERR_OR_NULL(client)) {
		pr_err("%s: trying to mmap an ion handle in a process with no "
		       "ion client\n", __func__);
		return -EINVAL;
	}

	if ((size > buffer->size) || (size + (vma->vm_pgoff << PAGE_SHIFT) >
				     buffer->size)) {
		pr_err("%s: trying to map larger area than handle has available"
		       "\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	/* find the handle and take a reference to it */
	handle = ion_import(client, buffer);
	if (IS_ERR_OR_NULL(handle)) {
		ret = -EINVAL;
		goto err;
	}

	if (!handle->buffer->heap->ops->map_user) {
		pr_err("%s: this heap does not define a method for mapping "
		       "to userspace\n", __func__);
		ret = -EINVAL;
		goto err1;
	}

	mutex_lock(&buffer->lock);

	if (ion_validate_buffer_flags(buffer, flags)) {
		ret = -EEXIST;
		mutex_unlock(&buffer->lock);
		goto err1;
	}

	/* now map it to userspace */
	ret = buffer->heap->ops->map_user(buffer->heap, buffer, vma,
						flags);
	mutex_unlock(&buffer->lock);
	if (ret) {
		pr_err("%s: failure mapping buffer to userspace\n",
		       __func__);
		goto err2;
	}

	vma->vm_ops = &ion_vm_ops;
	/* move the handle into the vm_private_data so we can access it from
	   vma_open/close */
	vma->vm_private_data = handle;
	pr_debug("%s: %d client_cnt %d handle_cnt %d alloc_cnt %d\n",
		 __func__, __LINE__,
		 atomic_read(&client->ref.refcount),
		 atomic_read(&handle->ref.refcount),
		 atomic_read(&buffer->ref.refcount));
	return 0;

err2:
	buffer->umap_cnt--;
	/* drop the reference to the handle */
err1:
	ion_handle_put(handle);
err:
	/* drop the reference to the client */
	ion_client_put(client);
	return ret;
}

static const struct file_operations ion_share_fops = {
	.owner		= THIS_MODULE,
	.release	= ion_share_release,
	.mmap		= ion_share_mmap,
};

static int ion_ioctl_share(struct file *parent, struct ion_client *client,
			   struct ion_handle *handle)
{
	int fd = get_unused_fd();
	struct file *file;

	if (fd < 0)
		return -ENFILE;

	file = anon_inode_getfile("ion_share_fd", &ion_share_fops,
				  handle->buffer, O_RDWR);
	if (IS_ERR_OR_NULL(file))
		goto err;

	if (parent->f_flags & O_DSYNC)
		file->f_flags |= O_DSYNC;

	ion_buffer_get(handle->buffer);
	fd_install(fd, file);

	return fd;

err:
	put_unused_fd(fd);
	return -ENFILE;
}

static long ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ion_client *client = filp->private_data;

	switch (cmd) {
	case ION_IOC_ALLOC:
	{
		struct ion_allocation_data data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;
		data.handle = ion_alloc(client, data.len, data.align,
					     data.flags);

		if (IS_ERR_OR_NULL(data.handle))
			return PTR_ERR(data.handle);

		if (copy_to_user((void __user *)arg, &data, sizeof(data)))
			return -EFAULT;
		break;
	}
	case ION_IOC_FREE:
	{
		struct ion_handle_data data;
		bool valid;

		if (copy_from_user(&data, (void __user *)arg,
				   sizeof(struct ion_handle_data)))
			return -EFAULT;
		mutex_lock(&client->lock);
		valid = ion_handle_validate(client, data.handle);
		mutex_unlock(&client->lock);
		if (!valid)
			return -EINVAL;
		ion_free(client, data.handle);
		break;
	}
	case ION_IOC_MAP:
	case ION_IOC_SHARE:
	{
		struct ion_fd_data data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;
		mutex_lock(&client->lock);
		if (!ion_handle_validate(client, data.handle)) {
			pr_err("%s: invalid handle passed to share ioctl.\n",
			       __func__);
			mutex_unlock(&client->lock);
			return -EINVAL;
		}
		data.fd = ion_ioctl_share(filp, client, data.handle);
		mutex_unlock(&client->lock);
		if (copy_to_user((void __user *)arg, &data, sizeof(data)))
			return -EFAULT;
		break;
	}
	case ION_IOC_IMPORT:
	{
		struct ion_fd_data data;
		if (copy_from_user(&data, (void __user *)arg,
				   sizeof(struct ion_fd_data)))
			return -EFAULT;

		data.handle = ion_import_fd(client, data.fd);
		if (IS_ERR(data.handle))
			data.handle = NULL;
		if (copy_to_user((void __user *)arg, &data,
				 sizeof(struct ion_fd_data)))
			return -EFAULT;
		break;
	}
	case ION_IOC_CUSTOM:
	{
		struct ion_device *dev = client->dev;
		struct ion_custom_data data;

		if (!dev->custom_ioctl)
			return -ENOTTY;
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(struct ion_custom_data)))
			return -EFAULT;
		return dev->custom_ioctl(client, data.cmd, data.arg);
	}
	case ION_IOC_CLEAN_CACHES:
	case ION_IOC_INV_CACHES:
	case ION_IOC_CLEAN_INV_CACHES:
	{
		struct ion_flush_data data;
		unsigned long start, end;
		struct ion_handle *handle = NULL;
		int ret;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(struct ion_flush_data)))
			return -EFAULT;

		start = (unsigned long) data.vaddr;
		end = (unsigned long) data.vaddr + data.length;

		if (check_vaddr_bounds(start, end)) {
			pr_err("%s: virtual address %p is out of bounds\n",
				__func__, data.vaddr);
			return -EINVAL;
		}

		if (!data.handle) {
			handle = ion_import_fd(client, data.fd);
			if (IS_ERR_OR_NULL(handle)) {
				pr_info("%s: Could not import handle: %d\n",
					__func__, (int)handle);
				return -EINVAL;
			}
		}

		ret = ion_do_cache_op(client,
					data.handle ? data.handle : handle,
					data.vaddr, data.offset, data.length,
					cmd);

		if (!data.handle)
			ion_free(client, handle);

		break;

	}
	case ION_IOC_GET_FLAGS:
	{
		struct ion_flag_data data;
		int ret;
		if (copy_from_user(&data, (void __user *)arg,
				   sizeof(struct ion_flag_data)))
			return -EFAULT;

		ret = ion_handle_get_flags(client, data.handle, &data.flags);
		if (ret < 0)
			return ret;
		if (copy_to_user((void __user *)arg, &data,
				 sizeof(struct ion_flag_data)))
			return -EFAULT;
		break;
	}
	default:
		return -ENOTTY;
	}
	return 0;
}

static int ion_release(struct inode *inode, struct file *file)
{
	struct ion_client *client = file->private_data;

	pr_debug("%s: %d\n", __func__, __LINE__);
	ion_client_put(client);
	return 0;
}

static int ion_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct ion_device *dev = container_of(miscdev, struct ion_device, dev);
	struct ion_client *client;
	char debug_name[64];

	pr_debug("%s: %d\n", __func__, __LINE__);
	snprintf(debug_name, 64, "%u", task_pid_nr(current->group_leader));
	client = ion_client_create(dev, -1, debug_name);
	if (IS_ERR_OR_NULL(client))
		return PTR_ERR(client);
	file->private_data = client;

	return 0;
}

static const struct file_operations ion_fops = {
	.owner          = THIS_MODULE,
	.open           = ion_open,
	.release        = ion_release,
	.unlocked_ioctl = ion_ioctl,
};

static size_t ion_debug_heap_total(struct ion_client *client,
				   enum ion_heap_ids id)
{
	size_t size = 0;
	struct rb_node *n;

	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n,
						     struct ion_handle,
						     node);
		if (handle->buffer->heap->id == id)
			size += handle->buffer->size;
	}
	mutex_unlock(&client->lock);
	return size;
}

static int ion_debug_heap_show(struct seq_file *s, void *unused)
{
	struct ion_heap *heap = s->private;
	struct ion_device *dev = heap->dev;
	struct rb_node *n;

	seq_printf(s, "%16.s %16.s %16.s\n", "client", "pid", "size");
	for (n = rb_first(&dev->user_clients); n; n = rb_next(n)) {
		struct ion_client *client = rb_entry(n, struct ion_client,
						     node);
		char task_comm[TASK_COMM_LEN];
		size_t size = ion_debug_heap_total(client, heap->id);
		if (!size)
			continue;

		get_task_comm(task_comm, client->task);
		seq_printf(s, "%16.s %16u %16x\n", task_comm, client->pid,
			   size);
	}

	for (n = rb_first(&dev->kernel_clients); n; n = rb_next(n)) {
		struct ion_client *client = rb_entry(n, struct ion_client,
						     node);
		size_t size = ion_debug_heap_total(client, heap->id);
		if (!size)
			continue;
		seq_printf(s, "%16.s %16u %16x\n", client->name, client->pid,
			   size);
	}
	if (heap->ops->get_allocated) {
		seq_printf(s, "total bytes currently allocated: %lx\n",
			heap->ops->get_allocated(heap));
	}
	if (heap->ops->get_total) {
		seq_printf(s, "total heap size: %lx\n",
			heap->ops->get_total(heap));
	}
	return 0;
}

static int ion_debug_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_heap_show, inode->i_private);
}

static const struct file_operations debug_heap_fops = {
	.open = ion_debug_heap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void ion_device_add_heap(struct ion_device *dev, struct ion_heap *heap)
{
	struct rb_node **p = &dev->heaps.rb_node;
	struct rb_node *parent = NULL;
	struct ion_heap *entry;

	heap->dev = dev;
	mutex_lock(&dev->lock);
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_heap, node);

		if (heap->id < entry->id) {
			p = &(*p)->rb_left;
		} else if (heap->id > entry->id ) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: can not insert multiple heaps with "
				"id %d\n", __func__, heap->id);
			goto end;
		}
	}

	rb_link_node(&heap->node, parent, p);
	rb_insert_color(&heap->node, &dev->heaps);
	debugfs_create_file(heap->name, 0664, dev->debug_root, heap,
			    &debug_heap_fops);
end:
	mutex_unlock(&dev->lock);
}

int ion_secure_heap(struct ion_device *dev, int heap_id)
{
	struct rb_node *n;
	int ret_val = 0;

	/*
	 * traverse the list of heaps available in this system
	 * and find the heap that is specified.
	 */
	mutex_lock(&dev->lock);
	for (n = rb_first(&dev->heaps); n != NULL; n = rb_next(n)) {
		struct ion_heap *heap = rb_entry(n, struct ion_heap, node);
		if (heap->type != ION_HEAP_TYPE_CP)
			continue;
		if (ION_HEAP(heap->id) != heap_id)
			continue;
		if (heap->ops->secure_heap)
			ret_val = heap->ops->secure_heap(heap);
		else
			ret_val = -EINVAL;
		break;
	}
	mutex_unlock(&dev->lock);
	return ret_val;
}

int ion_unsecure_heap(struct ion_device *dev, int heap_id)
{
	struct rb_node *n;
	int ret_val = 0;

	/*
	 * traverse the list of heaps available in this system
	 * and find the heap that is specified.
	 */
	mutex_lock(&dev->lock);
	for (n = rb_first(&dev->heaps); n != NULL; n = rb_next(n)) {
		struct ion_heap *heap = rb_entry(n, struct ion_heap, node);
		if (heap->type != ION_HEAP_TYPE_CP)
			continue;
		if (ION_HEAP(heap->id) != heap_id)
			continue;
		if (heap->ops->secure_heap)
			ret_val = heap->ops->unsecure_heap(heap);
		else
			ret_val = -EINVAL;
		break;
	}
	mutex_unlock(&dev->lock);
	return ret_val;
}

static int ion_debug_leak_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = s->private;
	struct rb_node *n;
	struct rb_node *n2;

	/* mark all buffers as 1 */
	seq_printf(s, "%16.s %16.s %16.s %16.s\n", "buffer", "heap", "size",
		"ref cnt");
	mutex_lock(&dev->lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buf = rb_entry(n, struct ion_buffer,
						     node);

		buf->marked = 1;
	}

	/* now see which buffers we can access */
	for (n = rb_first(&dev->kernel_clients); n; n = rb_next(n)) {
		struct ion_client *client = rb_entry(n, struct ion_client,
						     node);

		mutex_lock(&client->lock);
		for (n2 = rb_first(&client->handles); n2; n2 = rb_next(n2)) {
			struct ion_handle *handle = rb_entry(n2,
						struct ion_handle, node);

			handle->buffer->marked = 0;

		}
		mutex_unlock(&client->lock);

	}

	for (n = rb_first(&dev->user_clients); n; n = rb_next(n)) {
		struct ion_client *client = rb_entry(n, struct ion_client,
						     node);

		mutex_lock(&client->lock);
		for (n2 = rb_first(&client->handles); n2; n2 = rb_next(n2)) {
			struct ion_handle *handle = rb_entry(n2,
						struct ion_handle, node);

			handle->buffer->marked = 0;

		}
		mutex_unlock(&client->lock);

	}
	/* And anyone still marked as a 1 means a leaked handle somewhere */
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buf = rb_entry(n, struct ion_buffer,
						     node);

		if (buf->marked == 1)
			seq_printf(s, "%16.x %16.s %16.x %16.d\n",
				(int)buf, buf->heap->name, buf->size,
				atomic_read(&buf->ref.refcount));
	}
	mutex_unlock(&dev->lock);
	return 0;
}

static int ion_debug_leak_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_leak_show, inode->i_private);
}

static const struct file_operations debug_leak_fops = {
	.open = ion_debug_leak_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};



struct ion_device *ion_device_create(long (*custom_ioctl)
				     (struct ion_client *client,
				      unsigned int cmd,
				      unsigned long arg))
{
	struct ion_device *idev;
	int ret;

	idev = kzalloc(sizeof(struct ion_device), GFP_KERNEL);
	if (!idev)
		return ERR_PTR(-ENOMEM);

	idev->dev.minor = MISC_DYNAMIC_MINOR;
	idev->dev.name = "ion";
	idev->dev.fops = &ion_fops;
	idev->dev.parent = NULL;
	ret = misc_register(&idev->dev);
	if (ret) {
		pr_err("ion: failed to register misc device.\n");
		return ERR_PTR(ret);
	}

	idev->debug_root = debugfs_create_dir("ion", NULL);
	if (IS_ERR_OR_NULL(idev->debug_root))
		pr_err("ion: failed to create debug files.\n");

	idev->custom_ioctl = custom_ioctl;
	idev->buffers = RB_ROOT;
	mutex_init(&idev->lock);
	idev->heaps = RB_ROOT;
	idev->user_clients = RB_ROOT;
	idev->kernel_clients = RB_ROOT;
	debugfs_create_file("check_leaked_fds", 0664, idev->debug_root, idev,
			    &debug_leak_fops);
	return idev;
}

void ion_device_destroy(struct ion_device *dev)
{
	misc_deregister(&dev->dev);
	/* XXX need to free the heaps and clients ? */
	kfree(dev);
}
