#include <linux/vmalloc.h>
#include <linux/mm.h>

#include "bq27x00_dffs_SL3770125.h"
#include "bq27x00_dffs_WD3870127P.h"

#define MAX_NAME_LEN  20
#define MAX_OEM_STRING_LEN 8
#define MAX_PROD_NAME_LEN 4

struct bq27x00_dffs_list {
	char name[MAX_NAME_LEN+1];
	char oemstr[MAX_OEM_STRING_LEN+1];
	unsigned short fuel_gauge_type;
	int (*dffs)[10];
};

static const struct bq27x00_dffs_list bq27x00_dffs_data[] = {
	{
		.name = "SL3770125",
		.oemstr = SL3770125_DFFS_OEM_STR_G1,
		.fuel_gauge_type = 0x0202,
		.dffs = SL3770125_dffs_G1, 
	},
	{
		.name = "SL3770125",
		.oemstr = SL3770125_DFFS_OEM_STR_G2A,
		.fuel_gauge_type = 0x0205,
		.dffs = SL3770125_dffs_G2A, 
	},
	{
		.name = "WD3870127P",
		.oemstr = WD3870127P_DFFS_OEM_STR_G1,
		.fuel_gauge_type = 0x0202,
		.dffs = WD3870127P_dffs_G1, 
	},

	{
		.name = "WD3870127P",
		.oemstr = WD3870127P_DFFS_OEM_STR_G2A,
		.fuel_gauge_type = 0x0205,
		.dffs = WD3870127P_dffs_G2A, 
	},
};

void bq27x00_dffs_ls(char* output_str)
{
	int i,n;
	int v, y, m, d;
	char prod_name[MAX_PROD_NAME_LEN+1];
	char *p;

	p = output_str;
	n = sizeof(bq27x00_dffs_data)/sizeof(struct bq27x00_dffs_list);
	for (i=0; i<n; i++) {
		v = bq27x00_dffs_data[i].oemstr[0];
		
		memcpy(prod_name, &bq27x00_dffs_data[i].oemstr[1], MAX_PROD_NAME_LEN);
		prod_name[MAX_PROD_NAME_LEN] = 0;

		y = bq27x00_dffs_data[i].oemstr[5];
		m = bq27x00_dffs_data[i].oemstr[6];
		d = bq27x00_dffs_data[i].oemstr[7];

		sprintf(p, "%s, \t0x%04x, \t%s, \t%d, %d-%d-%d\n", 
				bq27x00_dffs_data[i].name, 
				bq27x00_dffs_data[i].fuel_gauge_type, 
				prod_name, 
				v, 
				y, m, d);
		p += strlen(p);
	}
}

void* bq27x00_dffs_get(char* model, unsigned short fuel_gauge_type)
{
	int i,n;
	
	n = sizeof(bq27x00_dffs_data)/sizeof(struct bq27x00_dffs_list);
	for (i=0; i<n; i++) {
		if (!strncmp(model, bq27x00_dffs_data[i].name, strlen(bq27x00_dffs_data[i].name)) && 
				(bq27x00_dffs_data[i].fuel_gauge_type == fuel_gauge_type)) {
			break;
		}
	}

	if (i < n) 
		return bq27x00_dffs_data[i].dffs;
	else
		return (void*)0; 
}

void bq27x00_dffs_find_by_oemstr(char* oemstr, char* output_str)
{
	int i,n;
	int v, y, m, d;
	char prod_name[MAX_PROD_NAME_LEN+1];

	n = sizeof(bq27x00_dffs_data)/sizeof(struct bq27x00_dffs_list);
	for (i=0; i<n; i++) {
		if (memcmp(oemstr, bq27x00_dffs_data[i].oemstr, MAX_OEM_STRING_LEN) == 0) {
			v = bq27x00_dffs_data[i].oemstr[0];
		
			memcpy(prod_name, &bq27x00_dffs_data[i].oemstr[1], MAX_PROD_NAME_LEN);
			prod_name[MAX_PROD_NAME_LEN] = 0;

			y = bq27x00_dffs_data[i].oemstr[5];
			m = bq27x00_dffs_data[i].oemstr[6];
			d = bq27x00_dffs_data[i].oemstr[7];

			sprintf(output_str, "%s, \t%04x, \t%s, \t%d, %d-%d-%d\n", 
					bq27x00_dffs_data[i].name, 
					bq27x00_dffs_data[i].fuel_gauge_type, 
					prod_name, 
					v, 
					y, m, d);
			break;
		}
	}
}

