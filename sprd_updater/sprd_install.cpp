#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <selinux/label.h>
#include <selinux/selinux.h>
#include <ftw.h>
#include <sys/statfs.h>
#include <sys/capability.h>
#include <sys/xattr.h>
#include <linux/xattr.h>
#include <inttypes.h>

#include "bootloader.h"
#include "applypatch/applypatch.h"
#include "cutils/android_reboot.h"
#include "cutils/misc.h"
#include "cutils/properties.h"
#include "edify/include/edify/expr.h"
#include "otautil/include/otautil/error_code.h"

#include "install/include/install/install.h"
#include "updater/include/updater/updater.h"

#define MAX_READ 1024
static char cmdline[MAX_READ+1] = {0};

/* SPRD: add for support check_sd_avail_space_enough @{ */
size_t AvailSpaceForFile(const char* filename) {
    struct statfs sf = {};

    if (statfs(filename, &sf) != 0) {
        printf("failed to statfs %s: %s\n", filename, strerror(errno));
        return -1;
    }
    return sf.f_bsize * sf.f_bavail / 1024 / 1024;
}

size_t UsedSpaceForFile(const char* filename) {
    struct statfs sf = {};

    if (statfs(filename, &sf) != 0) {
        printf("failed to statfs %s: %s\n", filename, strerror(errno));
        return 0;
    }
    return sf.f_bsize * (sf.f_blocks - sf.f_bfree) / 1024 / 1024 ;
}

Value* CheckPathSpaceEnoughFn(const char* name, State* state, const std::vector<std::unique_ptr<Expr>>& argv){
    size_t sd_avail_size = 0;
    size_t total_back_size = 0;
    size_t back_size = 0;
    size_t index_backup_item = 0;

	std::vector<std::string> args;
	if (!ReadArgs(state, argv, &args)) {
		return ErrorAbort(state, kArgsParsingFailure, "%s() Failed to parse the argument(s)", name);
	}
	sd_avail_size = AvailSpaceForFile((const char*)args[0].c_str());

    for(index_backup_item = 1;index_backup_item < argv.size(); index_backup_item++) {
        back_size = UsedSpaceForFile((const char*)args[index_backup_item].c_str());
        printf("back_size for %s = %dM\n", args[index_backup_item].c_str(), back_size);
        //free(args[index_backup_item].c_str());
        total_back_size += back_size;
    }

    printf("sd_avail_size = %dM, total_back_size = %dM\n", sd_avail_size, total_back_size);
    return StringValue(strdup(sd_avail_size > total_back_size ? "t" : ""));
}
/* @} */

/* SPRD: add for check system space if enough @{ */
Value* CheckSystemSpaceEnoughFn(const char* name, State* state, const std::vector<std::unique_ptr<Expr>>& argv){
    size_t need_size = 0;
    char * bytes_str;
    char * endptr;
    char * is_full_ota;
    size_t system_size = 0;
    int    success = 1;

	std::vector<std::string> args;
	if (!ReadArgs(state, argv, &args)) {
		return ErrorAbort(state, kArgsParsingFailure, "%s() Failed to parse the argument(s)", name);
	}
	bytes_str = (char *)args[0].c_str();
	is_full_ota = (char *)args[1].c_str();

    need_size = strtol(bytes_str, &endptr, 10);
    if (need_size == 0 && endptr == bytes_str) {
        success = 0;
        goto done;
    }
    need_size = need_size / 1024 / 1024;
    system_size = AvailSpaceForFile("/system");
    if (0 == strcmp(is_full_ota,"full_ota")) {
        system_size += UsedSpaceForFile("/system");
    }
    printf("system_size = %dM, need_size = %dM\n", system_size, need_size);
done:
    free(bytes_str);
    free(is_full_ota);
    free(endptr);
    if(success)
        return StringValue(strdup(system_size > need_size ? "t" : ""));
    else
        return ErrorAbort(state, " can't parse bytes_str as byte count!\n");
}
/* @} */

/* SPRD: add for support copy files @{ */
int copy_file(const char *src_file, const char *dst_file) {
    FILE *fp_src, *fp_dst;
    const int buf_size = 1024;
    char buf[buf_size];
    memset(buf, 0, buf_size);
    int size = 0;
    if((fp_src = fopen(src_file, "rb")) == NULL) {
        printf("source file can't open!\n");
        return -1;
    }
    if((fp_dst = fopen(dst_file, "wb+")) == NULL) {
        printf("target file can't open!\n");
        fclose(fp_src);
        return -1;
    }
    size = fread(buf, 1, buf_size, fp_src);
    while(size != 0) {
        fwrite(buf, 1, buf_size, fp_dst);
        size = fread(buf, 1, buf_size, fp_src);
    }
    fclose(fp_src);
    fclose(fp_dst);
    return 0;
}

Value* CopyFn(const char* name, State* state, const std::vector<std::unique_ptr<Expr>>& argv) {
    char* src_name;
    char* dst_name;
    int    success = 1;

	std::vector<std::string> args;
	if (!ReadArgs(state, argv, &args)) {
		return ErrorAbort(state, kArgsParsingFailure, "%s() Failed to parse the argument(s)", name);
	}
	src_name = (char *)args[0].c_str();
	dst_name = (char *)args[1].c_str();

    if (strlen(src_name) == 0) {
        ErrorAbort(state, "src_name argument to %s() can't be empty", name);
        goto done;
    }
    if (strlen(dst_name) == 0) {
        ErrorAbort(state, "dst_name argument to %s() can't be empty", name);
        goto done;
    }
    if ((success = copy_file(src_name, dst_name)) != 0) {
        ErrorAbort(state, "copy of %s to %s failed, error %s",
          src_name, dst_name, strerror(errno));
    }
done:
    free(src_name);
    free(dst_name);
    return StringValue(strdup(success ? "": "t"));
}
/* @} */

/* SPRD: get freespace of sdcard @{ */
//apply_disk_space(bytes)
Value* ApplyDiskSpaceFn(const char* name, State* state, const std::vector<std::unique_ptr<Expr>>& argv){
    char * bytes_str;
    char * path;

	std::vector<std::string> args;
	if (!ReadArgs(state, argv, &args)) {
		return ErrorAbort(state, kArgsParsingFailure, "%s() Failed to parse the argument(s)", name);
	}
	path = (char *)args[0].c_str();
	bytes_str = (char *)args[1].c_str();

    char * endptr;
    size_t need_size = strtol(bytes_str, &endptr, 10);
    if (need_size == 0 && endptr == bytes_str) {
        ErrorAbort(state, "%s(): can't parse \"%s\" as byte count\n\n",
                   name, bytes_str);
        free(bytes_str);
        return NULL;
    }
    need_size = need_size / 1024 / 1024;
    size_t sdsize = AvailSpaceForFile(path);
    printf("sdsize = %dM, need_size = %dM\n", sdsize, need_size);
    return StringValue(strdup(sdsize > need_size ? "t" : ""));
}
/* @} */

/* SPRD: add for support backup and resume data @{ */
// if file exist return "True" else return "False".
Value* ExistFn(const char* name, State* state, const std::vector<std::unique_ptr<Expr>>& argv) {
    char* result = NULL;
    struct stat st = {};

	std::vector<std::string> args;
	if (!ReadArgs(state, argv, &args)) {
		return ErrorAbort(state, kArgsParsingFailure, "%s() Failed to parse the argument(s)", name);
	}

    if (stat(args[0].c_str(), &st) != 0) {
        printf("failed to stat \"%s\": %s\n", args[0].c_str(), strerror(errno));
    }

    return StringValue(stat(args[0].c_str(), &st) != 0 ? "False" : "True");
}
/* @} */

// SPRD: add for update from 4.4 to 5.0
static int run_program_status = 0;

Value* RunProgramExFn(const char* name, State* state, const std::vector<std::unique_ptr<Expr>>& argv) {
	std::vector<std::string> args;
	if (!ReadArgs(state, argv, &args)) {
		return ErrorAbort(state, kArgsParsingFailure, "%s() Failed to parse the argument(s)", name);
	}
	char *program = (char*)args[0].c_str();

    char** args2 = (char **)malloc(sizeof(char*) * (argv.size()+1));
	for(unsigned int i = 0; i<argv.size(); i++){
		args2[i] = (char*)args[i].c_str();
        printf("args2[%d] = %s \n", i, args2[i]);
	}
	args2[argv.size()] = NULL;

    printf("start to run program [%s] with %d args\n", program, args.size());

    chmod(program, 0755);

    pid_t child = fork();
    if (child == 0) {
        execv(program, args2);
        printf("run_program: execv failed: %s\n", strerror(errno));
        _exit(1);
    }
    int status;
    waitpid(child, &status, 0);
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0) {
            printf("run_program: child exited with status %d\n",
                    WEXITSTATUS(status));
            run_program_status = WEXITSTATUS(status);
        }
    } else if (WIFSIGNALED(status)) {
        printf("run_program: child terminated by signal %d\n",
                WTERMSIG(status));
        run_program_status = 0;
    }

    free(args2);
    char buffer[20];
    sprintf(buffer, "%d", status);
    return StringValue(strdup(buffer));
}

/* SPRD: add for update from 4.4 to 5.0 @{ */
Value* LastRunStatusFn(const char* name, State* state, const std::vector<std::unique_ptr<Expr>>& argv) {
    char buffer[20] = {0};
	std::vector<std::string> args;

	if (!ReadArgs(state, argv, &args)) {
		return ErrorAbort(state, kArgsParsingFailure, "%s() Failed to parse the argument(s)", name);
	}
    sprintf(buffer, "%d", run_program_status);

    return StringValue(strdup(buffer));
}
/* @} */

//return value: 0 == boca rf, otherwise 1 == navajo rf type
Value* RunGetRftypeFn(const char* name, State* state, const std::vector<std::unique_ptr<Expr>>& argv) {
    std::vector<std::string> args;
    static char buffer[4] = {0};
    static char *ptr = NULL;
    int rf_type = 0;
    FILE *cmd_fd = NULL;

    if (!ReadArgs(state, argv, &args)) {
        return ErrorAbort(state, kArgsParsingFailure, "%s() Failed to parse the argument(s)", name);
    }

    if(ptr){
      printf("RunGetRftypeFn:: rf_type : %s\n", buffer);
      return StringValue(strdup(buffer));
    }

    if (!(cmd_fd = fopen("/proc/cmdline", "r"))) {
        printf("RunGetRftypeFn:open cmdline error \n");
        return NULL;
    }

    while (fgets(cmdline, MAX_READ, cmd_fd)) {
        printf("RunGetRftypeFn:cmdline = %s !\n", cmdline);
        //rf.type=0  BOCA, rf.type=1  NAVAJO
        ptr = strstr(cmdline, "rf.type=");
        if (ptr){
            ptr += strlen("rf.type=");
            printf("RunGetRftypeFn: ptr = %s !\n", ptr);
            if(0 == strncmp(ptr, "0", 1)){
                rf_type = 0;
            }else if(0 == strncmp(ptr, "1", 1)){
                rf_type = 1;
            }else{
                rf_type = 255;
                ptr = NULL;
                fclose(cmd_fd);
                cmd_fd = NULL;
                printf("RunGetRftypeFn:: cmdline has invalid rf.type 255 \n");
                return NULL;
            }
            printf("RunGetRftypeFn:rf_type= %d !\n", rf_type);
            break;
        }
    }

    fclose(cmd_fd);
    cmd_fd = NULL;

    if (! ptr){
        printf("RunGetRftypeFn:: cmdline has no rf.type field\n");
        return NULL;
    }

    sprintf(buffer, "%d", rf_type);
    printf("RunGetRftypeFn:: buffer : %s\n", buffer);
    return StringValue(strdup(buffer));
}

void Register_libsprd_updater(){
    RegisterFunction("apply_disk_space", ApplyDiskSpaceFn);
    RegisterFunction("exist", ExistFn);
    RegisterFunction("copy_file", CopyFn);
    RegisterFunction("check_path_space_enough", CheckPathSpaceEnoughFn); 
    RegisterFunction("check_system_space_enough", CheckSystemSpaceEnoughFn);
    RegisterFunction("last_run_status", LastRunStatusFn);
    RegisterFunction("run_programex", RunProgramExFn);
    RegisterFunction("get_rftype", RunGetRftypeFn);
}
