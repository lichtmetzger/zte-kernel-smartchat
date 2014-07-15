/*COMMON*/
#define PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB          		0x1350
#define PRODUCT_ID_MS_ADB                      			0x1351
#define PRODUCT_ID_ADB                             			0x1352
#define PRODUCT_ID_MS                               			0x1353
#define PRODUCT_ID_MODEM_MS_ADB         		0x1354
#define PRODUCT_ID_MODEM_MS                 			0x1355

#define PRODUCT_ID_MS_CDROM                 			0x0083

#define PRODUCT_ID_DIAG_MODEM_NMEA   		0x0111
#define PRODUCT_ID_DIAG                           			0x0112	

/*froyo RNDIS*/
#define PRODUCT_ID_RNDIS_MS                 			0x1364
#define PRODUCT_ID_RNDIS_MS_ADB             		0x1364
#define PRODUCT_ID_RNDIS             				0x1365
#define PRODUCT_ID_RNDIS_ADB					0x1373

/*n600 pcui*/
#define PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB_AT   	0x1366
#define PRODUCT_ID_DIAG_MODEM_NMEA_MS_AT   		0x1367

/*DELL project*/
#define PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB_DELL          	0x1368
#define PRODUCT_ID_MS_ADB_DELL                      	0x1369
#define PRODUCT_ID_MS_DELL                               		0x1370
#define PRODUCT_ID_MODEM_MS_ADB_DELL         	0x1371
#define PRODUCT_ID_MODEM_MS_DELL                 	0x1372

#define PRODUCT_ID_MS_ADB_CLR                     0x0211
#define PRODUCT_ID_MS_CLR                     0x0226
#define PRODUCT_ID_ALL_INTERFACE_CLR        0x0212
#define PRODUCT_ID_DIAG_ADB_CLR                  0x0213
#define PRODUCT_ID_MODEM_MS_ADB_CLR       0x0214
#define PRODUCT_ID_MODEM_MS_CLR                0x0215

//for PIDs with ONDA vendor tag
#define PRODUCT_ID_MS_ADB_ONDA                     0x0221
#define PRODUCT_ID_MS_ONDA               0x0227
#define PRODUCT_ID_ALL_INTERFACE_ONDA        0x0222
#define PRODUCT_ID_MODEM_MS_ADB_ONDA       0x0224
#define PRODUCT_ID_MODEM_MS_ONDA                0x0225
#define PRODUCT_ID_RNDIS_ADB_ONDA       0x0220
#define PRODUCT_ID_RNDIS_ONDA          0x0223

static char* usb_functions_diag_modem_nmea_ms_adb[] = 
{
	"diag",
	"modem",
	"nmea",
	"usb_mass_storage",
	"adb",
};

static char* usb_functions_diag_modem_nmea_ms_adb_at[] = 
{
	"diag",
	"modem",
	"nmea",
	"usb_mass_storage",
	"adb",
	"at",
};

static char* usb_functions_diag_modem_nmea_ms_at[] = 
{
	"diag",
	"modem",
	"nmea",
	"usb_mass_storage",
	"at",
};



static char* usb_function_ms_adb[] =
{
	"usb_mass_storage",
	"adb",
};

static char* usb_function_adb[] =
{
	"adb",
};

static char* usb_function_ms[]=
{
	"usb_mass_storage",
};

static char* usb_function_modem_ms_adb[]=
{"modem",
 "usb_mass_storage",
 "adb",
};

static char* usb_function_modem_ms[] = 
{
	"modem",
	"usb_mass_storage",
};

static char* usb_function_diag_modem_nmea[]=
{
        "diag",
        "modem",
        "nmea",
};


static char* usb_function_diag[] =
{
	"diag",
};

static char* usb_function_rndis_ms[] =
{
	"rndis",
	"usb_mass_storage",
};

static char* usb_function_rndis_ms_adb[] =
{
	"rndis",
	"usb_mass_storage",
	"adb",
};

static char* usb_function_rndis[] = 
{
	"rndis",
};

static char* usb_function_rndis_adb[] = 
{
	"rndis",
	"adb",
};

static struct usb_ether_platform_data rndis_pdata = {
	/* ethaddr is filled by board_serialno_setup */
	.vendorID	= 0x19d2,
	.vendorDescr	= "ZTE Incorporated",
};


static struct android_usb_product usb_products[] = {
	{
		.product_id	= PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB,
		.num_functions	= ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_adb),
		.functions	= usb_functions_diag_modem_nmea_ms_adb,
		.adb_product_id	= PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB,
		.adb_num_functions = ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_adb),
		.adb_functions	= usb_functions_diag_modem_nmea_ms_adb,
	},
	

	{
		.product_id	= PRODUCT_ID_MS,
		.num_functions	= ARRAY_SIZE(usb_function_ms),
		.functions	= usb_function_ms,
		.adb_product_id	= PRODUCT_ID_MS_ADB,
		.adb_num_functions	= ARRAY_SIZE(usb_function_ms_adb),
		.adb_functions	= usb_function_ms_adb,
	},

	{
		.product_id	= PRODUCT_ID_MS_CDROM,
		.num_functions	= ARRAY_SIZE(usb_function_ms),
		.functions	= usb_function_ms,
		.adb_product_id	= PRODUCT_ID_MS_CDROM,
		.adb_num_functions	= ARRAY_SIZE(usb_function_ms),
		.adb_functions	= usb_function_ms,
	},	
	
	{
		.product_id	= PRODUCT_ID_ADB,
		.num_functions	= ARRAY_SIZE(usb_function_adb),
		.functions	= usb_function_adb,
		.adb_product_id	= PRODUCT_ID_ADB,
		.adb_num_functions	= ARRAY_SIZE(usb_function_adb),
		.adb_functions	= usb_function_adb,
	},
	
	{
		.product_id	= PRODUCT_ID_MODEM_MS,
		.num_functions	= ARRAY_SIZE(usb_function_modem_ms),
		.functions	= usb_function_modem_ms,
		.adb_product_id	= PRODUCT_ID_MODEM_MS_ADB,
		.adb_num_functions	= ARRAY_SIZE(usb_function_modem_ms_adb),
		.adb_functions	= usb_function_modem_ms_adb,
	},
	
	{
		.product_id     = PRODUCT_ID_DIAG_MODEM_NMEA,
		.num_functions  = ARRAY_SIZE(usb_function_diag_modem_nmea),
		.functions      = usb_function_diag_modem_nmea,
		.adb_product_id = PRODUCT_ID_DIAG_MODEM_NMEA,
		.adb_num_functions      = ARRAY_SIZE(usb_function_diag_modem_nmea),
		.adb_functions  = usb_function_diag_modem_nmea,
	},
	
	{
		.product_id = PRODUCT_ID_DIAG,
		.num_functions	= ARRAY_SIZE(usb_function_diag),
		.functions	= usb_function_diag,
		.adb_product_id = PRODUCT_ID_DIAG,
		.adb_num_functions	= ARRAY_SIZE(usb_function_diag),
		.adb_functions	= usb_function_diag,
	},
	
	{
		.product_id = PRODUCT_ID_RNDIS_MS,
		.num_functions	= ARRAY_SIZE(usb_function_rndis_ms),
		.functions	= usb_function_rndis_ms,
		.adb_product_id = PRODUCT_ID_RNDIS_MS_ADB,
		.adb_num_functions	= ARRAY_SIZE(usb_function_rndis_ms_adb),
		.adb_functions	= usb_function_rndis_ms_adb,
	},
		
	{
		.product_id = PRODUCT_ID_RNDIS,
		.num_functions	= ARRAY_SIZE(usb_function_rndis),
		.functions	= usb_function_rndis,
		.adb_product_id = PRODUCT_ID_RNDIS,
		.adb_num_functions	= ARRAY_SIZE(usb_function_rndis),
		.adb_functions	= usb_function_rndis,
	},
	{
		.product_id = PRODUCT_ID_RNDIS_ADB,
		.num_functions	= ARRAY_SIZE(usb_function_rndis_adb),
		.functions	= usb_function_rndis_adb,
		.adb_product_id = PRODUCT_ID_RNDIS_ADB,
		.adb_num_functions	= ARRAY_SIZE(usb_function_rndis_adb),
		.adb_functions	= usb_function_rndis_adb,
	},
	//dell
	{
		.product_id	= PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB_DELL,
		.num_functions	= ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_adb),
		.functions	= usb_functions_diag_modem_nmea_ms_adb,
		.adb_product_id	= PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB_DELL,
		.adb_num_functions = ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_adb),
		.adb_functions	= usb_functions_diag_modem_nmea_ms_adb,
	},
	{
		.product_id	= PRODUCT_ID_MS_DELL,
		.num_functions	= ARRAY_SIZE(usb_function_ms),
		.functions	= usb_function_ms,
		.adb_product_id	= PRODUCT_ID_MS_ADB_DELL,
		.adb_num_functions	= ARRAY_SIZE(usb_function_ms_adb),
		.adb_functions	= usb_function_ms_adb,
	},
	{
		.product_id	= PRODUCT_ID_MODEM_MS_DELL,
		.num_functions	= ARRAY_SIZE(usb_function_modem_ms),
		.functions	= usb_function_modem_ms,
		.adb_product_id	= PRODUCT_ID_MODEM_MS_ADB_DELL,
		.adb_num_functions	= ARRAY_SIZE(usb_function_modem_ms_adb),
		.adb_functions	= usb_function_modem_ms_adb,
	},
	//end

	{
		.product_id	= PRODUCT_ID_DIAG_MODEM_NMEA_MS_AT,
		.num_functions	= ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_at),
		.functions	= usb_functions_diag_modem_nmea_ms_at,
		.adb_product_id	= PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB_AT,
		.adb_num_functions = ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_adb_at),
		.adb_functions	= usb_functions_diag_modem_nmea_ms_adb_at,
	},

	{		
		.product_id	= PRODUCT_ID_MS_CLR,
		.num_functions	= ARRAY_SIZE(usb_function_ms),
		.functions	= usb_function_ms,
		.adb_product_id	= PRODUCT_ID_MS_ADB_CLR,
		.adb_num_functions = ARRAY_SIZE(usb_function_ms_adb),
		.adb_functions	= usb_function_ms_adb,		
	},
	{
		.product_id	= PRODUCT_ID_ALL_INTERFACE_CLR,
		.num_functions	= ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_adb),
		.functions	= usb_functions_diag_modem_nmea_ms_adb,
		.adb_product_id	= PRODUCT_ID_ALL_INTERFACE_CLR,
		.adb_num_functions = ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_adb),
		.adb_functions	= usb_functions_diag_modem_nmea_ms_adb,
	},
	{
		.product_id	= PRODUCT_ID_MODEM_MS_CLR,
		.num_functions	= ARRAY_SIZE(usb_function_modem_ms),
		.functions	= usb_function_modem_ms,
		.adb_product_id	= PRODUCT_ID_MODEM_MS_ADB_CLR,
		.adb_num_functions = ARRAY_SIZE(usb_function_modem_ms_adb),
		.adb_functions	= usb_function_modem_ms_adb,
	},	
        //for PIDs with ONDA vendor tag
        {
                /* ms+adb*/
                .product_id     = PRODUCT_ID_MS_ONDA,
                .num_functions  = ARRAY_SIZE(usb_function_ms),
                .functions      = usb_function_ms,
                .adb_product_id = PRODUCT_ID_MS_ADB_ONDA,
                .adb_num_functions = ARRAY_SIZE(usb_function_ms_adb),
                .adb_functions  = usb_function_ms_adb,

        },
        {
                /* diag+modm+nema+ms*/
                .product_id         = PRODUCT_ID_ALL_INTERFACE_ONDA,
                .num_functions  = ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_adb),
                .functions      = usb_functions_diag_modem_nmea_ms_adb,
                .adb_product_id = PRODUCT_ID_ALL_INTERFACE_ONDA,
                .adb_num_functions = ARRAY_SIZE(usb_functions_diag_modem_nmea_ms_adb),
                .adb_functions  = usb_functions_diag_modem_nmea_ms_adb,

        },
        {
                /* modm+ms+adb*/
                .product_id         = PRODUCT_ID_MODEM_MS_ONDA,
                .num_functions  = ARRAY_SIZE(usb_function_modem_ms),
                .functions      = usb_function_modem_ms,
                .adb_product_id = PRODUCT_ID_MODEM_MS_ADB_ONDA,
                .adb_num_functions = ARRAY_SIZE(usb_function_modem_ms_adb),
                .adb_functions  = usb_function_modem_ms_adb,
        },
        {
                /* rndis*/
                .product_id         = PRODUCT_ID_RNDIS_ONDA,
                .num_functions  = ARRAY_SIZE(usb_function_rndis),
                .functions      = usb_function_rndis,
                .adb_product_id = PRODUCT_ID_RNDIS_ONDA,
                .adb_num_functions      = ARRAY_SIZE(usb_function_rndis),
                .adb_functions  = usb_function_rndis,
        },
        {
                /* rndis+adb*/
                .product_id         = PRODUCT_ID_RNDIS_ADB_ONDA,
                .num_functions  = ARRAY_SIZE(usb_function_rndis),
                .functions      = usb_function_rndis,
                .adb_product_id = PRODUCT_ID_RNDIS_ADB_ONDA,
                .adb_num_functions      = ARRAY_SIZE(usb_function_rndis_adb),
                .adb_functions  = usb_function_rndis_adb,
        },
};

