#include <stdio.h>
#include <unistd.h>
#include "gptdata.h"
#include <stdlib.h>
#include <string.h>
#include "roots.h"
#include <selinux/android.h>
#include <selinux/label.h>
#include <selinux/selinux.h>

char * dev_name;
struct selabel_handle* sehandle;
int main(int argc, char *argv[])
{
    GPTData *mGPTData;
    char cfgfile[40] = {0};
    int is_check = 0;
    int ret = 1;
    int change_pos;
    int repart_flag = 0;
    char userdata_fs_type[16] = {0};

    printf("gpt partition enter!\n");

	sehandle = selinux_android_file_context_handle();
	selinux_android_set_sehandle(sehandle);
	if (!sehandle) {
		printf("Warning: No file_contexts\n");
	}

    dev_name = (char *)malloc(256);
    strcpy(dev_name, "/dev/block/mmcblk0");

    mGPTData = (GPTData *)malloc(sizeof(GPTData));
    if(NULL == mGPTData)
    {
        printf("malloc GPTData failer!\n");
        return RUN_ERROR;
    }
    memset(mGPTData, 0, sizeof(GPTData));

    InitializeGPTData(mGPTData);

    int ch;
    while ((ch = getopt(argc, argv, "b:c:d:k:f:")) != -1) {
        switch(ch) {
            case 'b':
                mGPTData->backup_dir = (char *)malloc(256);
                memset(mGPTData->backup_dir, 0, 256);
                strcpy(mGPTData->backup_dir, optarg);
                printf(" Have option -b is %s !\n", mGPTData->backup_dir);
                break;
            case 'c':
                printf(" Have option -c is %s !\n", optarg);
                strcpy(cfgfile, optarg);
                break;
            case 'd':
                strcpy(dev_name, optarg);
                printf(" Have option -d is %s !\n", dev_name);
                break;
            case 'k':
                is_check = atoi(optarg);
                printf(" Have option -k is %d !\n", is_check);
                break;
            case 'f':
                strcpy(userdata_fs_type, optarg);
                printf(" Have option -f is %s !\n", userdata_fs_type);
                break;
            case '?':
                printf("Unknown option: %c !\n", (char)optopt);
        }
    }

    if(!strcmp(cfgfile,"")) {
        printf("this tool must have -c option pointed a configfile!\n");
        return RUN_ERROR;
    }

    ret = ParseConfigFile(mGPTData, cfgfile); //parse config file
    if (ret == 0) {
        printf("Parse config file error!\n");
        return RUN_ERROR;
    }

    DisplayConfigFile(mGPTData);

    ret = LoadPartitions(mGPTData, dev_name);
    if(ret == 0) {
        printf("LoadPartitions failed!\n");
        return RUN_ERROR;
    }

    DisplayGPTData(mGPTData);

    change_pos = CheckPartitionChange(mGPTData);
    printf("change_pos=%d\n", change_pos);

    if (is_check) {
        if (change_pos < 0)
            return RUN_NO_CHANGE;
        else
            return RUN_CHANGE;
    }

    if (0 == RepartFlagFromBackdir(mGPTData->backup_dir, GET_FLAG, &repart_flag))
        return RUN_ERROR;

    if (change_pos < 0 && repart_flag == 0) {
        return RUN_NO_CHANGE;
    }

    mGPTData->repart_flag = repart_flag;

    ret = BackupPartData2File(mGPTData, change_pos);
    if(ret != RUN_SUCCESS)
        return ret;

    ret = ChangePartition(mGPTData, change_pos);
    if(ret != RUN_SUCCESS)
        return ret;

    DisplayGPTData(mGPTData);

    if (0 == RepartFlagFromBackdir(mGPTData->backup_dir, SET_FLAG, &repart_flag))
        return RUN_ERROR;

    if (ensure_path_unmounted("/cache") != 0) {
        printf("SaveGPTData: unmount cache failed!");
        return RUN_ERROR;
    }

    if (0 == SaveGPTData(mGPTData, change_pos))
        return RUN_ERROR;

    if (0 == IfNeedFormatPartition(mGPTData, change_pos, "cache", "/cache"))
        return RUN_ERROR;

    if (ensure_path_mounted("/cache") != 0) {
        printf("main: mount cache failed!");
        return RUN_ERROR;
    }
	printf("main: mount cache successed!");

    if (0 == RecoveryPartition(mGPTData))
        return RUN_ERROR;

	//if(userdata_fs_type[0] != '\0')
	    //change_userdata_volume_table(userdata_fs_type);
    if (0 == IfNeedFormatPartition(mGPTData, change_pos, "userdata", "/data"))
        return RUN_ERROR;

    FinalDestory(mGPTData);

    if (0 == RepartFlagFromBackdir(mGPTData->backup_dir, CLEAR_FLAG, &repart_flag))
        return RUN_ERROR;

    return RUN_SUCCESS;
}
