#ifdef CONFIG_BATTERY_MODEL_WD3870127P
	#define BATTERY_MODEL "WD3870127P"
#else
	#ifdef CONFIG_BATTERY_MODEL_SL3770125
		#define BATTERY_MODEL "SL3770125"
	#else
		#error "BATTERY_MODEL IS NOT DEFINED." 
	#endif
#endif

void bq27x00_dffs_ls(char* output_str);
void* bq27x00_dffs_get(char* model, unsigned short fuel_gauge_type);
void bq27x00_dffs_find_by_oemstr(char* oemstr, char* output_str);
