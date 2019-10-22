//
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t version;

    int year;
    int mon;
    int day;
    int hour;
    int min;
    int sec;
} date_time_t;

// valid string:
// image filename1 = "AABBCCDD.img"
// image filename2 = "AABBCCDD_YYYYmmdd.img"
// image filename3 = "AABBCCDD_YYYYmmdd_HHMMSS.img"
static int img_parse_filename(date_time_t *date_time, const char *filename)
{
    if (date_time == NULL || filename == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    char *name = strrchr(filename, '/');
    if (name != NULL) {
        filename = name + 1;
    }

    memset(date_time, 0, sizeof(date_time_t));
    date_time->year = 1900;
    date_time->mon = 1;
    date_time->day = 1;

    date_time->version = strtol(filename, NULL, 16);
    char *ymd = strchr(filename, '_');
    if (ymd != NULL) {
        uint32_t value = strtol(ymd+1, NULL, 10);
        date_time->year = value/10000;
        date_time->day = value%100;
        date_time->mon = (value%10000 - date_time->day)/100;

        char *hms = strchr(ymd+1, '_');
        if (hms != NULL) {
            uint32_t value = strtol(hms+1, NULL, 10);
            date_time->hour = value/10000;
            date_time->sec = value%100;
            date_time->min = (value%10000 - date_time->sec)/100;
        }
    }

    return 0;
}

// filename ==> version,time
int img_get_fileinfo(img_info_t *img, const char *filename)
{
    if (img == NULL || filename == NULL) {
        errno = EINVAL;
        return -1;
    }

    date_time_t date_time;
    img_parse_filename(&date_time, filename);

    struct tm stm;
    stm.tm_year     = date_time.year - 1900;
    stm.tm_mon      = date_time.mon - 1;
    stm.tm_mday     = date_time.day;
    stm.tm_hour     = date_time.hour;
    stm.tm_min      = date_time.min;
    stm.tm_sec      = date_time.sec;

    snprintf(img->filename, sizeof(img->filename), "%s", filename);
    img->time       = mktime(&stm);
    img->version    = date_time.version;
    img->algo       = IMG_VER_TO_ALGO(img->version);
    img->model      = IMG_VER_TO_MODEL(img->version);

    return 0;
}

// algo,model,time ==> filename
int img_scan_files(img_info_t *img, int algo, int model, time_t *tm)
{
    if (img == NULL) {
        errno = EINVAL;
        return -1;
    }

    DIR *dp;
    if ((dp = opendir(IMG_DIR)) == NULL) {
        return -1;
    }
    char bak_dir_buf[256];
    getcwd(bak_dir_buf, sizeof(bak_dir_buf));
    chdir(IMG_DIR);

    int ret = -1;
    errno = ENOENT;
    img_info_t info, ret_info;
    memset(&ret_info, 0, sizeof(img_info_t));

    struct dirent *entry;
    struct stat statbuf;
    while ((entry = readdir(dp)) != NULL) {
        lstat(entry->d_name, &statbuf);
        if (!(S_IFDIR & statbuf.st_mode)) {
            img_get_fileinfo(&info, entry->d_name);
            if ( info.algo == algo && info.model == model && (tm == NULL || info.time == *tm) ) {
                ret = 0;
                errno = 0;
                if (info.version > ret_info.version) {
                    memcpy(&ret_info, &info, sizeof(img_info_t));
                }
            }
        }
    }

    chdir(bak_dir_buf);
    closedir(dp);
    
    if (ret == 0) {
        memcpy(img, &ret_info, sizeof(img_info_t));
    }

    return ret;
}

#ifdef __cplusplus
}
#endif

