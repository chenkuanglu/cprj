/*
 * Copyright 2010 Jeff Garzik
 * Copyright 2012-2014 pooler
 * Copyright 2014 Lucas Jones
 * Copyright 2014 Tanguy Pruvot
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#include <cpuminer-config.h>
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#include <curl/curl.h>
#include <jansson.h>
#include <openssl/sha.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef _MSC_VER
#include <windows.h>
#include <stdint.h>
#else
#include <errno.h>
#if HAVE_SYS_SYSCTL_H
#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/sysctl.h>
#endif
#endif

#ifndef WIN32
#include <sys/resource.h>
#endif

#include "miner.h"

#include "fpga/middles.h"

#include "fpga/hash_fpga.h"
#include "fpga/uart_fpga.h"
#include "fpga/middles.h"
#include <sys/utsname.h>

#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<math.h>
#include <termios.h>
//#include <openssl/sha.h>

#ifdef WIN32
#include "compat/winansi.h"
BOOL WINAPI ConsoleHandler(DWORD);
#endif

#define LP_SCANTIME		60

#define	MAX_MSG_NUM		8

#define SCAN_MAGIC      "are you miner?"
#define REPLY_MAGIC     "cloudersemi"

#ifndef min
#define min(a,b) (a>b ? b : a)
#define max(a,b) (a<b ? b : a)
#endif

int g_opt_algo;
uint32_t g_request_id = 2500000;

enum workio_commands {
	WC_GET_WORK,
	WC_SUBMIT_WORK,
};

struct workio_cmd {
	enum workio_commands cmd;
	struct thr_info *thr;
	union {
		struct work *work;
	} u;
};


static const char *algo_names[] = {
	"keccak",
	"heavy",
	"neoscrypt",
	"quark",
	"axiom",
	"bastion",
	"blake",
	"blakecoin",
	"blake2s",
	"bmw",
	"c11",
	"cryptolight",
	"cryptonight",
	"decred",
	"dmd-gr",
	"drop",
	"fresh",
	"groestl",
	"jha",
	"lbry",
	"luffa",
	"lyra2re",
	"lyra2rev2",
	"lyra2rev3",
	"myr-gr",
	"nist5",
	"pentablake",
	"pluck",
	"qubit",
	"scrypt",
	"scrypt-jane",
	"shavite3",
	"sha256d",
	"sia",
	"sib",
	"skein",
	"skein2",
	"s3",
	"timetravel",
	"bitcore",
	"tribus",
	"vanilla",
	"veltor",
	"x11evo",
	"x11",
	"x13",
	"bcd",
	"x14",
	"x15",
	"x16r",
	"x16s",
	"hex",
	"x17",
	"phi2",
	"xevan",
	"yescrypt",
	"zr5",
	"keccakc",
	"blake2b",
	
	"phi1612",
	"skunk",
	"lyra2z",
	"cr200",
	"cr184",
	
	"comb",
    "vblake",
    "trigg",
	"\0"
};

miner_cb_t miner_cb[MAX_POOL_NUM];
middle_cb_t mid_cb[256];

uint32_t vblake_submit_success_count;
uint32_t vblake_submit_failure_count;

bool use_roots = false;
int comb_first_config = 0;
bool opt_debug = false;
bool opt_debug_diff = false;
bool opt_protocol = false;
bool opt_benchmark = false;
bool opt_redirect = true;
bool opt_showdiff = true;
bool opt_extranonce = true;
bool want_longpoll = true;
bool have_longpoll = false;
bool allow_getwork = true;
bool want_stratum = true;
bool have_stratum = false;
bool opt_stratum_stats = false;
bool allow_mininginfo = true;
bool use_syslog = false;
bool use_colors = true;
static bool opt_background = false;
bool opt_asic_mode = false;
bool opt_quiet = false;
int opt_maxlograte = 5;
bool opt_randomize = false;
static int opt_retries = 3;
static int opt_fail_pause = 5;
static double opt_accept_limit = 0.;
static bool high_accept_ever = false;    // 提交率曾经很高
static int opt_job_timeout = 300;	    //job 超时时间默认5分钟
static int opt_time_limit = 0;
int opt_timeout = 300;
static int opt_scantime = 60;
static const bool opt_time = true;
static int opt_scrypt_n = 1024;
static int opt_pluck_n = 128;
static unsigned int opt_nfactor = 6;
int opt_n_threads = 0;
int64_t opt_affinity = -1L;
int opt_priority = 0;
int num_cpus;
char *device_name;

static unsigned char pk_script[25] = { 0 };
static size_t pk_script_size = 0;
static char coinbase_sig[101] = { 0 };
char *opt_cert;
char *opt_proxy;
long opt_proxy_type;

struct thr_info *thr_info;
int work_thr_id;
int longpoll_thr_id = -1;
int stratum_thr_id = -1;
int api_thr_id = -1;
bool stratum_need_reset = false;
bool stratum_connected_ever = false;
struct stratum_ctx stratum_v[MAX_POOL_NUM];

int opt_url_cnt = 0;
int opt_algo_cnt = 0;
int middle_stage_ix = 0;
//int opt_num_cnt = 0;

bool jsonrpc_2 = false;
char rpc2_id[64] = "";
char *rpc2_blob = NULL;
size_t rpc2_bloblen = 0;
uint32_t rpc2_target = 0;
char *rpc2_job_id = NULL;
bool aes_ni_supported = false;

pthread_mutex_t rpc2_job_lock;
pthread_mutex_t rpc2_login_lock;
pthread_mutex_t applog_lock;
pthread_mutex_t stats_lock;
pthread_mutex_t miner_chain_mutex;
uint32_t zr5_pok = 0;

uint32_t submit_count = 0L;

uint32_t solved_count = 0L;
uint32_t accepted_count = 0L;
uint32_t rejected_count = 0L;
uint64_t global_hashrate = 0;
double stratum_diff = 0.;
double net_diff = 0.;
double net_hashrate = 0.;
uint64_t net_blocks = 0;
// conditional mining
bool conditional_state[MAX_CPUS] = { 0 };
double opt_max_temp = 0.0;
double opt_max_diff = 0.0;
double opt_max_rate = 0.0;
double opt_warn_temp= 1000.0;
uint32_t opt_frame_timeout = 3000;

uint32_t opt_work_size = 0; /* default */
char *opt_api_allow = NULL;
int opt_api_remote = 0;
int opt_api_listen = 5512; //4048; /* 0 to disable */
int opt_chip_num = 0;
int chip_num = 0;
struct work_restart *work_restart = NULL;

static struct miner_chip* miner_chip_head = NULL;
int fd_uart = 0;
static int miner_chip_count = 0;
static int host_type = 0;
static time_t startup_time = 0; 
static uint32_t hash_error_count = 0;
uint32_t hash_error_max = 0;
time_t hash_err_time = 0;
uint32_t hash_error_period = 60; //60s

double exp_rate = 0;
static long clr_interval = 0;

int chip_model = -1;

uint8_t middle_stage[MAX_POOL_NUM][256] = {0};

extern pthread_mutex_t	uart_mutex;
#define CHIP_CORE_NUM	8   // except xdag

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};
#endif

static char const usage[] = "\
Usage: " PACKAGE_NAME " [OPTIONS]\n\
Options:\n\
  -a, --algo=ALGO       specify the algorithm to use\n\
                          axiom        Shabal-256 MemoHash\n\
                          bitcore      Timetravel with 10 algos\n\
                          blake        Blake-256 14-rounds (SFR)\n\
                          blakecoin    Blake-256 single sha256 merkle\n\
                          blake2s      Blake2-S (256)\n\
                          blake2b      Blake2-B (256)\n\
                          bmw          BMW 256\n\
                          c11/flax     C11\n\
                          cryptolight  Cryptonight-light\n\
                          cryptonight  Monero\n\
                          decred       Blake-256 14-rounds 180 bytes\n\
                          dmd-gr       Diamond-Groestl\n\
                          drop         Dropcoin\n\
                          fresh        Fresh\n\
                          groestl      GroestlCoin\n\
                          heavy        Heavy\n\
                          jha          JHA\n\
                          keccak       Keccak\n\
                          keccakc      Keccakc\n\
                          luffa        Luffa\n\
                          lyra2re      Lyra2RE\n\
                          lyra2rev2    Lyra2REv2 (Vertcoin)\n\
                          lyra2rev3    Lyra2REv3 (Vertcoin)\n\
                          lyra2z       Lyra2z\n\
                          myr-gr       Myriad-Groestl\n\
                          neoscrypt    NeoScrypt(128, 2, 1)\n\
                          nist5        Nist5\n\
                          phi1612      Phi1612\n\
                          pluck        Pluck:128 (Supcoin)\n\
                          pentablake   Pentablake\n\
                          quark        Quark\n\
                          qubit        Qubit\n\
                          scrypt       scrypt(1024, 1, 1) (default)\n\
                          scrypt:N     scrypt(N, 1, 1)\n\
                          scrypt-jane:N (with N factor from 4 to 30)\n\
                          shavite3     Shavite3\n\
                          sha256d      SHA-256d\n\
                          sia          Blake2-B\n\
                          sib          X11 + gost (SibCoin)\n\
                          skein        Skein+Sha (Skeincoin)\n\
                          skein2       Double Skein (Woodcoin)\n\
                          skunk        Skunk\n\
                          s3           S3\n\
                          timetravel   Timetravel (Machinecoin)\n\
                          tribus       Tribus\n\
                          vanilla      Blake-256 8-rounds\n\
                          x11evo       Permuted x11\n\
                          x11          X11\n\
                          x13          X13\n\
                          bcd          X13(use sm3 instead of luffa)\n\
                          x14          X14\n\
                          x15          X15\n\
                          x16r         X16r\n\
                          x16s         X16s\n\
                          hex          X16m(XDNA)\n\
                          x17          X17\n\
                          phi2         phi2\n\
                          xevan        Xevan (BitSend)\n\
                          yescrypt     Yescrypt\n\
                          zr5          ZR5\n\
  -A,			ASIC mode\n\
  -C,			chip number\n\
  -o, --url=URL         URL of mining server\n\
  -O, --userpass=U:P    username:password pair for mining server\n\
  -u, --user=USERNAME   username for mining server\n\
  -p, --pass=PASSWORD   password for mining server\n\
      --cert=FILE       certificate for mining server using SSL\n\
  -x, --proxy=[PROTOCOL://]HOST[:PORT]  connect through a proxy\n\
  -r, --retries=N       number of times to retry if a network call fails\n\
                          (default: retry 3 times)\n\
  -R, --retry-pause=N   time to pause between retries, in seconds (default: 30)\n\
      --time-limit=N    maximum time [s] to mine before exiting the program.\n\
  -T, --timeout=N       timeout for long poll and stratum (default: 300 seconds)\n\
  -s, --scantime=N      upper bound on time spent scanning current work \n\
                          long polling is unavailable, in seconds (default: 5s)\n\
      --randomize       Randomize scan range start to reduce duplicates\n\
  -f, --diff-factor     Divide req. difficulty by this factor (std is 1.0)\n\
  -F,			accept ratio limit, if bwlow this, then exit appliction(default is 0)\n\
  -J, --job timeout	job timeout, if bigger than this, then exit appliction(default: 300)\n\
  -m, --diff-multiplier Multiply difficulty by this factor (std is 1.0)\n\
  -n, --nfactor         neoscrypt N-Factor\n\
      --coinbase-addr=ADDR  payout address for solo mining\n\
      --coinbase-sig=TEXT  data to insert in the coinbase when possible\n\
      --max-log-rate    limit per-core hashrate logs (default: 5s)\n\
      --no-longpoll     disable long polling support\n\
      --no-getwork      disable getwork support\n\
      --no-gbt          disable getblocktemplate support\n\
      --no-stratum      disable X-Stratum support\n\
      --no-extranonce   disable Stratum extranonce support\n\
      --no-redirect     ignore requests to change the URL of the mining server\n\
  -q, --quiet           disable per-thread hashmeter output\n\
      --no-color        disable colored output\n\
  -d, --device          serial port device name (default: /dev/ttyUSB0) \n\
  -D, --debug           enable debug output\n\
  -P, --protocol-dump   verbose dump of protocol-level activities\n\
      --hide-diff       Hide submitted block and net difficulty\n"
#ifdef HAVE_SYSLOG_H
"\
  -S, --syslog          use system log for output messages\n"
#endif
"\
  -B, --background      run the miner in the background\n\
      --benchmark       run in offline benchmark mode\n\
      --cputest         debug hashes from cpu algorithms\n\
      --cpu-affinity    set process affinity to cpu core(s), mask 0x3 for cores 0 and 1\n\
      --cpu-priority    set process priority (default: 0 idle, 2 normal to 5 highest)\n\
  -b, --api-bind        IP/Port for the miner API (default: 0.0.0.0:5512, allow all ip)\n\
      --api-remote      Allow remote control\n\
      --max-temp=N      Only mine if cpu temp is less than specified value (linux)\n\
      --warn-temp       warn temperature higher than specified value\n\
      --max-rate=N[KMG] Only mine if net hashrate is less than specified value\n\
      --max-diff=N      Only mine if net difficulty is less than specified value\n\
  -c, --config=FILE     load a JSON-format configuration file\n\
  -V, --version         display version information and exit\n\
  -h, --help            display this help text and exit\n\
";


static char const short_options[] =
#ifdef HAVE_SYSLOG_H
	"S"
#endif
	"M:a:Ab:Bc:C:d:Df:F:hjJ:m:n:p:Px:qr:R:s:T:o:u:O:V";

static struct option const options[] = {
	{ "middle", 1, NULL, 'M' },
	{ "algo", 1, NULL, 'a' },
	{ "api-bind", 1, NULL, 'b' },
	{ "hash-error-max", 1, NULL, 1101 },
	{ "hash-error-period", 1, NULL, 1102 },
	{ "api-remote", 0, NULL, 1030 },
	{ "background", 0, NULL, 'B' },
	{ "benchmark", 0, NULL, 1005 },
	{ "cputest", 0, NULL, 1006 },
	{ "cert", 1, NULL, 1001 },
	{ "coinbase-addr", 1, NULL, 1016 },
	{ "coinbase-sig", 1, NULL, 1015 },
	{ "config", 1, NULL, 'c' },
	{ "cpu-affinity", 1, NULL, 1020 },
	{ "cpu-priority", 1, NULL, 1021 },
	{ "no-color", 0, NULL, 1002 },
	{ "debug", 0, NULL, 'D' },
	{ "diff-factor", 1, NULL, 'f' },
	{ "diff", 1, NULL, 'f' }, // deprecated (alias)
	{ "diff-multiplier", 1, NULL, 'm' },
	{ "help", 0, NULL, 'h' },
	{ "nfactor", 1, NULL, 'n' },
	{ "no-gbt", 0, NULL, 1011 },
	{ "no-getwork", 0, NULL, 1010 },
	{ "no-longpoll", 0, NULL, 1003 },
	{ "no-redirect", 0, NULL, 1009 },
	{ "no-stratum", 0, NULL, 1007 },
	{ "no-extranonce", 0, NULL, 1012 },
	{ "max-temp", 1, NULL, 1060 },
	{ "max-diff", 1, NULL, 1061 },
	{ "max-rate", 1, NULL, 1062 },
	{ "pass", 1, NULL, 'p' },
	{ "protocol", 0, NULL, 'P' },
	{ "protocol-dump", 0, NULL, 'P' },
	{ "proxy", 1, NULL, 'x' },
	{ "quiet", 0, NULL, 'q' },
	{ "retries", 1, NULL, 'r' },
	{ "retry-pause", 1, NULL, 'R' },
	{ "randomize", 0, NULL, 1024 },
	{ "scantime", 1, NULL, 's' },
	{ "show-diff", 0, NULL, 1013 },
	{ "hide-diff", 0, NULL, 1014 },
	{ "max-log-rate", 1, NULL, 1019 },
#ifdef HAVE_SYSLOG_H
	{ "syslog", 0, NULL, 'S' },
#endif
	{ "time-limit", 1, NULL, 1008 },
	{ "timeout", 1, NULL, 'T' },
	{ "url", 1, NULL, 'o' },
	{ "user", 1, NULL, 'u' },
	{ "userpass", 1, NULL, 'O' },
	{ "version", 0, NULL, 'V' },
	{ "warn-temp", 1, NULL, 2000 },
	{ 0, 0, 0, 0 }
};

static struct work g_work_v[MAX_POOL_NUM] = { 0 };
static time_t g_work_time_v[MAX_POOL_NUM] = { 0 };
static time_t g_clean_time_v[MAX_POOL_NUM] = { 0 };
static pthread_mutex_t g_work_lock_v[MAX_POOL_NUM];
static bool submit_old = false;
static char *lp_id;

static void workio_cmd_free(struct workio_cmd *wc);
static void clear_all_miners(int algo);

#ifdef __linux /* Linux specific policy and affinity management */
#include <sched.h>

static inline void drop_policy(void)
{
	struct sched_param param;
	param.sched_priority = 0;
#ifdef SCHED_IDLE
	if (unlikely(sched_setscheduler(0, SCHED_IDLE, &param) == -1))
#endif
#ifdef SCHED_BATCH
		sched_setscheduler(0, SCHED_BATCH, &param);
#endif
}

#ifdef __BIONIC__
#define pthread_setaffinity_np(tid,sz,s) {} /* only do process affinity */
#endif

static void affine_to_cpu_mask(int id, unsigned long mask) {
	cpu_set_t set;
	CPU_ZERO(&set);
	for (uint8_t i = 0; i < num_cpus; i++) {
		// cpu mask
		if (mask & (1UL<<i)) { CPU_SET(i, &set); }
	}
	if (id == -1) {
		// process affinity
		sched_setaffinity(0, sizeof(&set), &set);
	} else {
		// thread only
		pthread_setaffinity_np(thr_info[id].pth, sizeof(&set), &set);
	}
}

#elif defined(WIN32) /* Windows */
static inline void drop_policy(void) { }
static void affine_to_cpu_mask(int id, unsigned long mask) {
	if (id == -1)
		SetProcessAffinityMask(GetCurrentProcess(), mask);
	else
		SetThreadAffinityMask(GetCurrentThread(), mask);
}
#else
static inline void drop_policy(void) { }
static void affine_to_cpu_mask(int id, unsigned long mask) { }
#endif

// no one use
void get_currentalgo(char* buf, int sz)
{
    
	if (miner_cb[0].opt_algo == ALGO_SCRYPTJANE)
		snprintf(buf, sz, "%s:%d", algo_names[miner_cb[0].opt_algo], opt_scrypt_n);
	else
		snprintf(buf, sz, "%s", algo_names[miner_cb[0].opt_algo]);
    
}

int write_runtime_csv(int err, char* exit_reason)
{
	int is_pool_switch = 0;

	switch(err)
	{
	case ERR_NONE:
		break;
	case ERR_CRITICAL:
		break;
	case ERR_UART_FRAME:
		break;
	case ERR_CHIP_TIMEOUT:
		break;
	case ERR_UART_OPEN:	
		break;
	case ERR_ALGO:
		break;
	case ERR_IMG_VER:
		break;
	case ERR_POOL_TIMEOUT:
		is_pool_switch = 1;
		break;
	case ERR_LOW_ACCEPT:
		is_pool_switch = 1;
		break;
	case ERR_JOB_TIMEOUT:
		is_pool_switch = 1;
		break;
	default:;
	}

	int result = 0;
	time_t now_time;
	char* tmp = malloc(1024);
	
	if (NULL == tmp)
	{
		printf("write runtime.csv fail for malloc menmory fail");
		result = -1;
		return result;
	}

	#define MAX_SIZE 512
	char current_absolute_path[MAX_SIZE];
	memset(current_absolute_path, 0, MAX_SIZE);

	//获取当前程序绝对路径
	int cnt = readlink("/proc/self/exe", current_absolute_path, MAX_SIZE);
	if (cnt < 0 || cnt >= MAX_SIZE)
	{
		printf("get miner path fail");
		result = -2;
		return result;
	}
	//获取当前目录绝对路径，即去掉程序名
	for (int i = cnt; i >=0; --i)
	{
	    if (current_absolute_path[i] == '/')
	    {
	        current_absolute_path[i+1] = '\0';
	        break;
	    }
	}
	//printf("current absolute path:%s\n", current_absolute_path);	

	char* filename = "runtime.csv";

	memcpy(current_absolute_path + strlen(current_absolute_path), filename, strlen(filename) + 1); 

	/* 所有用户可读写权限 */
	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd <=0 )
	{
		printf("open %s fail, code = %d\n", current_absolute_path, fd);
		result = -2;
		goto EXIT;
	}
	ftruncate(fd,0);

	// 没有pool_switch这个参数了, 但是保留位置，用于以后其他参数的添加
	//sprintf(tmp, "pool_switch,%s\r\n", is_pool_switch?"true":"false");
	sprintf(tmp, "reserved,%s\r\n", is_pool_switch?" ":" ");
	result = write(fd, tmp, strlen(tmp));
	if (result <=0 )
	{
		printf("write runtime.csv fail, code = %d\r\n", result);
		result = -3;
		goto EXIT;
	}

	time(&now_time);
	sprintf(tmp, "pool_uptime,%d\r\n", (int)(now_time - startup_time));
	result = write(fd, tmp, strlen(tmp));
	if (result <=0 )
	{
		printf("write fail, code = %d\r\n", result);
		result = -4;
		goto EXIT;
	}

	sprintf(tmp, "pool_err,%d\r\n", rejected_count);
	result = write(fd, tmp, strlen(tmp));
	if (result <=0 )
	{
		printf("write fail, code = %d\r\n", result);
		result = -5;
		goto EXIT;
	}

	sprintf(tmp, "pool_success,%d\r\n", accepted_count);
	result = write(fd, tmp, strlen(tmp));
	if (result <=0 )
	{
		printf("write fail, code = %d\r\n", result);
		result = -6;
		goto EXIT;
	}

	sprintf(tmp, "exit_reason,%s\r\n", exit_reason);
	result = write(fd, tmp, strlen(tmp));
	if (result <=0 )
	{
		printf("write fail, code = %d\r\n", result);
		result = -7;
		goto EXIT;
	}

	/* 关闭文件 */
	result = close(fd);
	if (result < 0 )
	{
		printf("close fail, code = %d\n", result);
		result = -8;
		goto EXIT;
	}
	result = 0;

EXIT:
	free(tmp);
	return result;
}

void proper_exit(int code)
{
#ifdef WIN32
	if (opt_background) {
		HWND hcon = GetConsoleWindow();
		if (hcon) {
			// unhide parent command line windows
			ShowWindow(hcon, SW_SHOWMINNOACTIVE);
		}
	}
#endif
	char* exit_reason = NULL;
	switch(code)
	{
	case ERR_NONE:
		exit_reason = "ok";
		break;
	case ERR_CRITICAL:
		exit_reason = "critical";
		break;
	case ERR_UART_FRAME:
		exit_reason = "uart frame";
		break;
	case ERR_CHIP_TIMEOUT:
		exit_reason = "chip timeout";
		break;
	case ERR_UART_OPEN:	
		exit_reason = "uart open error";
		break;
	case ERR_ALGO:
		exit_reason = "algo not supported";
		break;
	case ERR_IMG_VER:
		//exit_reason = "image version dismatch";
		exit_reason = "drop into golden";
		break;
	case ERR_POOL_TIMEOUT:
		exit_reason = "pool timeout";
		break;
	case ERR_POOL_DISCONNECTED:
		exit_reason = "pool disconnected";
		break;
	case ERR_POOL_UNREACHABLE:
		exit_reason = "pool unreachable";
		break;
	case ERR_LOW_ACCEPT:
		exit_reason = "low pool accepted rate";
		break;
    case ERR_DROP_ACCEPT:
		exit_reason = "drop-down pool accepted rate";
		break;		
	case ERR_JOB_TIMEOUT:
		exit_reason = "get job timeout";
		break;
	case ERR_FIRST_HASH:
	    exit_reason = "first hash error";
	    break;
	case ERR_HASH_ERROR:
	    exit_reason = "too many hash error";
	    break;
	    
	default:
		exit_reason = "null";
	}

    broadcast_clear_chip(fd_uart);    // broadcast 'stop' cmd
	applog_exit(LOG_ERR, "exit, code %d: %s\n", code, exit_reason);
    //broadcast_clear_chip(fd_uart);    // broadcast 'stop' cmd
    //sleep(5);
    //fpga_hardware_reset();
    applog_disable();

	switch(code)
	{
	case ERR_UART_OPEN:	
	case ERR_ALGO:
		break;
	default:
        break;
	}

	write_runtime_csv(code, exit_reason);
	exit(code);
}

static inline void work_free(struct work *w)
{
	if (w->txs) free(w->txs);
	if (w->workid) free(w->workid);
	if (w->job_id) free(w->job_id);
	if (w->xnonce2) free(w->xnonce2);
}

static inline void work_copy(struct work *dest, const struct work *src)
{
	memcpy(dest, src, sizeof(struct work));
	if (src->txs)
		dest->txs = strdup(src->txs);
	if (src->workid)
		dest->workid = strdup(src->workid);
	if (src->job_id)
		dest->job_id = strdup(src->job_id);
	if (src->xnonce2) {
		dest->xnonce2 = (uchar*) malloc(src->xnonce2_len);
		memcpy(dest->xnonce2, src->xnonce2, src->xnonce2_len);
	}
}

/* compute nbits to get the network diff */
static void calc_network_diff(struct work *work)
{
    int opt_algo = miner_cb[work->res_id].opt_algo;

	// sample for diff 43.281 : 1c05ea29
	// todo: endian reversed on longpoll could be zr5 specific...
	uint32_t nbits = have_longpoll ? work->data[18] : swab32(work->data[18]);
	if (opt_algo == ALGO_LBRY) nbits = swab32(work->data[26]);
	if (opt_algo == ALGO_DECRED) nbits = work->data[29];
	if (opt_algo == ALGO_SIA) nbits = work->data[11]; // unsure if correct
	uint32_t bits = (nbits & 0xffffff);
	int16_t shift = (swab32(nbits) & 0xff); // 0x1c = 28

	double d = (double)0x0000ffff / (double)bits;
	for (int m=shift; m < 29; m++) d *= 256.0;
	for (int m=29; m < shift; m++) d /= 256.0;
	if (opt_algo == ALGO_DECRED && shift == 28) d *= 256.0; // testnet
	if (opt_debug_diff)
		applog(LOG_DEBUG, "net diff: %f -> shift %u, bits %08x", d, shift, bits);
	net_diff = d;
}

static bool work_decode(const json_t *val, struct work *work)
{
    int opt_algo = miner_cb[work->res_id].opt_algo;

	int i;
	int data_size = 128, target_size = sizeof(work->target);
	int adata_sz = 32, atarget_sz = ARRAY_SIZE(work->target);

	if (opt_algo == ALGO_DROP || opt_algo == ALGO_NEOSCRYPT || opt_algo == ALGO_ZR5) {
		data_size = 80; target_size = 32;
		adata_sz = 20;
		atarget_sz = target_size /  sizeof(uint32_t);
	} else if (opt_algo == ALGO_DECRED) {
		allow_mininginfo = false;
		data_size = 192;
		adata_sz = 180/4;
    } else if (use_roots) {
        data_size = 144;
        adata_sz = 36;
    }

	if (jsonrpc_2) {
		return rpc2_job_decode(val, work);
	}

	if (unlikely(!jobj_binary(val, "data", work->data, data_size))) {
		applog(LOG_ERR, "JSON invalid data");
		goto err_out;
	}
	if (unlikely(!jobj_binary(val, "target", work->target, target_size))) {
		applog(LOG_ERR, "JSON invalid target");
		goto err_out;
	}

	for (i = 0; i < adata_sz; i++)
		work->data[i] = le32dec(work->data + i);
	for (i = 0; i < atarget_sz; i++)
		work->target[i] = le32dec(work->target + i);

	if ((opt_showdiff || opt_max_diff > 0.) && !allow_mininginfo)
		calc_network_diff(work);

	work->targetdiff = target_to_diff(work->target);

	// for api stats, on longpoll pools
	stratum_diff = work->targetdiff;

	if (opt_algo == ALGO_DROP || opt_algo == ALGO_ZR5) {
		#define POK_BOOL_MASK 0x00008000
		#define POK_DATA_MASK 0xFFFF0000
		if (work->data[0] & POK_BOOL_MASK) {
			applog(LOG_BLUE, "POK received: %08xx", work->data[0]);
			zr5_pok = work->data[0] & POK_DATA_MASK;
		}
	} else if (opt_algo == ALGO_DECRED) {
		// some random extradata to make the work unique
		work->data[36] = (rand()*4);
		work->height = work->data[32];
		// required for the getwork pools (multicoin.co)
		if (!have_longpoll && work->height > net_blocks + 1) {
			char netinfo[64] = { 0 };
			if (opt_showdiff && net_diff > 0.) {
				if (net_diff != work->targetdiff)
					sprintf(netinfo, ", diff %.3f, target %.1f", net_diff, work->targetdiff);
				else
					sprintf(netinfo, ", diff %.3f", net_diff);
			}
			applog(LOG_BLUE, "%s block %d%s",
				algo_names[opt_algo], work->height, netinfo);
			net_blocks = work->height - 1;
		}
    } else if (opt_algo == ALGO_PHI2) {                                                                                                                
        if (work->data[0] & (1<<30)) use_roots = true;
        else for (i = 20; i < 36; i++) {
           if (work->data[i]) { use_roots = true; break; }
        }
    }

	return true;

err_out:
	return false;
}

// good alternative for wallet mining, difficulty and net hashrate
static const char *info_req =
"{\"method\": \"getmininginfo\", \"params\": [], \"id\":8}\r\n";

static bool get_mininginfo(CURL *curl, struct work *work)
{
    char* rpc_userpass = miner_cb[work->res_id].rpc_userpass;
    int opt_algo = miner_cb[work->res_id].opt_algo;
    char *rpc_url = miner_cb[work->res_id].rpc_url;
    bool have_gbt = miner_cb[work->res_id].have_gbt;

	if (have_stratum || have_longpoll || !allow_mininginfo)
		return false;

	int curl_err = 0;
	json_t *val = json_rpc_call(have_gbt, curl, rpc_url, rpc_userpass, info_req, &curl_err, 0);

	if (!val && curl_err == -1) {
		allow_mininginfo = false;
		if (opt_debug) {
			applog(LOG_DEBUG, "getmininginfo not supported");
		}
		return false;
	}
	else {
		json_t *res = json_object_get(val, "result");
		// "blocks": 491493 (= current work height - 1)
		// "difficulty": 0.99607860999999998
		// "networkhashps": 56475980
		if (res) {
			json_t *key = json_object_get(res, "difficulty");
			if (key) {
				if (json_is_object(key))
					key = json_object_get(key, "proof-of-work");
				if (json_is_real(key))
					net_diff = json_real_value(key);
			}
			key = json_object_get(res, "networkhashps");
			if (key && json_is_integer(key)) {
				net_hashrate = (double) json_integer_value(key);
			}
			key = json_object_get(res, "blocks");
			if (key && json_is_integer(key)) {
				net_blocks = json_integer_value(key);
			}
			if (!work->height) {
				// complete missing data from getwork
				work->height = (uint32_t) net_blocks + 1;
				if (work->height > g_work_v[0].height) {
					clear_all_miners(opt_algo);
					if (!opt_quiet) {
						char netinfo[64] = { 0 };
						char srate[32] = { 0 };
						sprintf(netinfo, "diff %.2f", net_diff);
						if (net_hashrate) {
							format_hashrate(net_hashrate, srate);
							strcat(netinfo, ", net ");
							strcat(netinfo, srate);
						}
						applog(LOG_BLUE, "%s block %d, %s",
							algo_names[opt_algo], work->height, netinfo);
					}
				}
			}
		}
	}
	json_decref(val);
	return true;
}

#define BLOCK_VERSION_CURRENT 3

static bool gbt_work_decode(const json_t *val, struct work *work)
{
    char *rpc_url = miner_cb[work->res_id].rpc_url;

	int i, n;
	uint32_t version, curtime, bits;
	uint32_t prevhash[8];
	uint32_t target[8];
	int cbtx_size;
	uchar *cbtx = NULL;
	int tx_count, tx_size;
	uchar txc_vi[9];
	uchar(*merkle_tree)[32] = NULL;
	bool coinbase_append = false;
	bool submit_coinbase = false;
	bool version_force = false;
	bool version_reduce = false;
	json_t *tmp, *txa;
	bool rc = false;
    bool have_gbt = miner_cb[work->res_id].have_gbt;

	tmp = json_object_get(val, "mutable");
	if (tmp && json_is_array(tmp)) {
		n = (int) json_array_size(tmp);
		for (i = 0; i < n; i++) {
			const char *s = json_string_value(json_array_get(tmp, i));
			if (!s)
				continue;
			if (!strcmp(s, "coinbase/append"))
				coinbase_append = true;
			else if (!strcmp(s, "submit/coinbase"))
				submit_coinbase = true;
			else if (!strcmp(s, "version/force"))
				version_force = true;
			else if (!strcmp(s, "version/reduce"))
				version_reduce = true;
		}
	}

	tmp = json_object_get(val, "height");
	if (!tmp || !json_is_integer(tmp)) {
		applog(LOG_ERR, "JSON invalid height");
		goto out;
	}
	work->height = (int) json_integer_value(tmp);
	applog(LOG_BLUE, "Current block is %d", work->height);

	tmp = json_object_get(val, "version");
	if (!tmp || !json_is_integer(tmp)) {
		applog(LOG_ERR, "JSON invalid version");
		goto out;
	}
	version = (uint32_t) json_integer_value(tmp);
	if ((version & 0xffU) > BLOCK_VERSION_CURRENT) {
		if (version_reduce) {
			version = (version & ~0xffU) | BLOCK_VERSION_CURRENT;
		} else if (have_gbt && allow_getwork && !version_force) {
			applog(LOG_DEBUG, "Switching to getwork, gbt version %d", version);
			have_gbt = false;
			goto out;
		} else if (!version_force) {
			applog(LOG_ERR, "Unrecognized block version: %u", version);
			goto out;
		}
	}

	if (unlikely(!jobj_binary(val, "previousblockhash", prevhash, sizeof(prevhash)))) {
		applog(LOG_ERR, "JSON invalid previousblockhash");
		goto out;
	}

	tmp = json_object_get(val, "curtime");
	if (!tmp || !json_is_integer(tmp)) {
		applog(LOG_ERR, "JSON invalid curtime");
		goto out;
	}
	curtime = (uint32_t) json_integer_value(tmp);

	if (unlikely(!jobj_binary(val, "bits", &bits, sizeof(bits)))) {
		applog(LOG_ERR, "JSON invalid bits");
		goto out;
	}

	/* find count and size of transactions */
	txa = json_object_get(val, "transactions");
	if (!txa || !json_is_array(txa)) {
		applog(LOG_ERR, "JSON invalid transactions");
		goto out;
	}
	tx_count = (int) json_array_size(txa);
	tx_size = 0;
	for (i = 0; i < tx_count; i++) {
		const json_t *tx = json_array_get(txa, i);
		const char *tx_hex = json_string_value(json_object_get(tx, "data"));
		if (!tx_hex) {
			applog(LOG_ERR, "JSON invalid transactions");
			goto out;
		}
		tx_size += (int) (strlen(tx_hex) / 2);
	}

	/* build coinbase transaction */
	tmp = json_object_get(val, "coinbasetxn");
	if (tmp) {
		const char *cbtx_hex = json_string_value(json_object_get(tmp, "data"));
		cbtx_size = cbtx_hex ? (int) strlen(cbtx_hex) / 2 : 0;
		cbtx = (uchar*) malloc(cbtx_size + 100);
		if (cbtx_size < 60 || !hex2bin(cbtx, cbtx_hex, cbtx_size)) {
			applog(LOG_ERR, "JSON invalid coinbasetxn");
			goto out;
		}
	} else {
		int64_t cbvalue;
		if (!pk_script_size) {
			if (allow_getwork) {
				applog(LOG_INFO, "No payout address provided, switching to getwork");
				have_gbt = false;
			} else
				applog(LOG_ERR, "No payout address provided");
			goto out;
		}
		tmp = json_object_get(val, "coinbasevalue");
		if (!tmp || !json_is_number(tmp)) {
			applog(LOG_ERR, "JSON invalid coinbasevalue");
			goto out;
		}
		cbvalue = (int64_t) (json_is_integer(tmp) ? json_integer_value(tmp) : json_number_value(tmp));
		cbtx = (uchar*) malloc(256);
		le32enc((uint32_t *)cbtx, 1); /* version */
		cbtx[4] = 1; /* in-counter */
		memset(cbtx+5, 0x00, 32); /* prev txout hash */
		le32enc((uint32_t *)(cbtx+37), 0xffffffff); /* prev txout index */
		cbtx_size = 43;
		/* BIP 34: height in coinbase */
		for (n = work->height; n; n >>= 8)
			cbtx[cbtx_size++] = n & 0xff;
		cbtx[42] = cbtx_size - 43;
		cbtx[41] = cbtx_size - 42; /* scriptsig length */
		le32enc((uint32_t *)(cbtx+cbtx_size), 0xffffffff); /* sequence */
		cbtx_size += 4;
		cbtx[cbtx_size++] = 1; /* out-counter */
		le32enc((uint32_t *)(cbtx+cbtx_size), (uint32_t)cbvalue); /* value */
		le32enc((uint32_t *)(cbtx+cbtx_size+4), cbvalue >> 32);
		cbtx_size += 8;
		cbtx[cbtx_size++] = (uint8_t) pk_script_size; /* txout-script length */
		memcpy(cbtx+cbtx_size, pk_script, pk_script_size);
		cbtx_size += (int) pk_script_size;
		le32enc((uint32_t *)(cbtx+cbtx_size), 0); /* lock time */
		cbtx_size += 4;
		coinbase_append = true;
	}
	if (coinbase_append) {
		unsigned char xsig[100];
		int xsig_len = 0;
		if (*coinbase_sig) {
			n = (int) strlen(coinbase_sig);
			if (cbtx[41] + xsig_len + n <= 100) {
				memcpy(xsig+xsig_len, coinbase_sig, n);
				xsig_len += n;
			} else {
				applog(LOG_WARNING, "Signature does not fit in coinbase, skipping");
			}
		}
		tmp = json_object_get(val, "coinbaseaux");
		if (tmp && json_is_object(tmp)) {
			void *iter = json_object_iter(tmp);
			while (iter) {
				unsigned char buf[100];
				const char *s = json_string_value(json_object_iter_value(iter));
				n = s ? (int) (strlen(s) / 2) : 0;
				if (!s || n > 100 || !hex2bin(buf, s, n)) {
					applog(LOG_ERR, "JSON invalid coinbaseaux");
					break;
				}
				if (cbtx[41] + xsig_len + n <= 100) {
					memcpy(xsig+xsig_len, buf, n);
					xsig_len += n;
				}
				iter = json_object_iter_next(tmp, iter);
			}
		}
		if (xsig_len) {
			unsigned char *ssig_end = cbtx + 42 + cbtx[41];
			int push_len = cbtx[41] + xsig_len < 76 ? 1 :
			               cbtx[41] + 2 + xsig_len > 100 ? 0 : 2;
			n = xsig_len + push_len;
			memmove(ssig_end + n, ssig_end, cbtx_size - 42 - cbtx[41]);
			cbtx[41] += n;
			if (push_len == 2)
				*(ssig_end++) = 0x4c; /* OP_PUSHDATA1 */
			if (push_len)
				*(ssig_end++) = xsig_len;
			memcpy(ssig_end, xsig, xsig_len);
			cbtx_size += n;
		}
	}

	n = varint_encode(txc_vi, 1 + tx_count);
	work->txs = (char*) malloc(2 * (n + cbtx_size + tx_size) + 1);
	bin2hex(work->txs, txc_vi, n);
	bin2hex(work->txs + 2*n, cbtx, cbtx_size);

	/* generate merkle root */
	merkle_tree = (uchar(*)[32]) calloc(((1 + tx_count + 1) & ~1), 32);
	sha256d(merkle_tree[0], cbtx, cbtx_size);
	for (i = 0; i < tx_count; i++) {
		tmp = json_array_get(txa, i);
		const char *tx_hex = json_string_value(json_object_get(tmp, "data"));
		const int tx_size = tx_hex ? (int) (strlen(tx_hex) / 2) : 0;
		unsigned char *tx = (uchar*) malloc(tx_size);
		if (!tx_hex || !hex2bin(tx, tx_hex, tx_size)) {
			applog(LOG_ERR, "JSON invalid transactions");
			free(tx);
			goto out;
		}
		sha256d(merkle_tree[1 + i], tx, tx_size);
		if (!submit_coinbase)
			strcat(work->txs, tx_hex);
	}
	n = 1 + tx_count;
	while (n > 1) {
		if (n % 2) {
			memcpy(merkle_tree[n], merkle_tree[n-1], 32);
			++n;
		}
		n /= 2;
		for (i = 0; i < n; i++)
			sha256d(merkle_tree[i], merkle_tree[2*i], 64);
	}

	/* assemble block header */
	work->data[0] = swab32(version);
	for (i = 0; i < 8; i++)
		work->data[8 - i] = le32dec(prevhash + i);
	for (i = 0; i < 8; i++)
		work->data[9 + i] = be32dec((uint32_t *)merkle_tree[0] + i);
	work->data[17] = swab32(curtime);
	work->data[18] = le32dec(&bits);
	memset(work->data + 19, 0x00, 52);

	work->data[20] = 0x80000000;
	work->data[31] = 0x00000280;

	if (unlikely(!jobj_binary(val, "target", target, sizeof(target)))) {
		applog(LOG_ERR, "JSON invalid target");
		goto out;
	}
	for (i = 0; i < ARRAY_SIZE(work->target); i++)
		work->target[7 - i] = be32dec(target + i);

	tmp = json_object_get(val, "workid");
	if (tmp) {
		if (!json_is_string(tmp)) {
			applog(LOG_ERR, "JSON invalid workid");
			goto out;
		}
		work->workid = strdup(json_string_value(tmp));
	}

	rc = true;
out:
	/* Long polling */
	tmp = json_object_get(val, "longpollid");
	if (want_longpoll && json_is_string(tmp)) {
		free(lp_id);
		lp_id = strdup(json_string_value(tmp));
		if (!have_longpoll) {
			char *lp_uri;
			tmp = json_object_get(val, "longpolluri");
			lp_uri = json_is_string(tmp) ? strdup(json_string_value(tmp)) : rpc_url;
			have_longpoll = true;
			tq_push(thr_info[longpoll_thr_id].q, lp_uri);
		}
	}

	free(merkle_tree);
	free(cbtx);
	return rc;
}

static double calc_all_hashrate(void)
{
	double acc = 0;
	struct miner_chip** miner_chip;

	pthread_mutex_lock(&miner_chain_mutex);

	for (miner_chip = &miner_chip_head; NULL != *miner_chip; miner_chip = &(*miner_chip)->next)
	{
		acc += (*miner_chip)->chip_info.hashrate;
	}

	pthread_mutex_unlock(&miner_chain_mutex);

	return acc;
}

#define YES "yes!"
#define YAY "yay!!!"
#define BOO "booooo"

// 这个值越大，则速率越稳定，但不利于实时反映速率的变化
// 这个值越小，则速率波动越大，但利于反映实时速率的变化
#define SUM_NUM         ( 512 )
#define SUM_NUM_MAX     ( 100 * 256 )

static double sum_time = 0;
static double sum_diff = 0;  

// 当yes个数较大时，不利于实时速率的统计，即速率很难变化
// 所以，程序总是采样最新的 N (=200)个yes进行速率统计
static double average_ev(double time, double diff)
{
    static double sum_buf[SUM_NUM_MAX*2];   // [time, diff]
    static int rdx = 0;
    static int wrx = 0;

    static uint64_t hashrate = 1;

    if (submit_count > 1)
    {
        // 预存 SUM_NUM 个理想值
        // 当芯片过多时，有的已经提交多个，而有的芯片还没有提交，所以：
        // 只要提交个数不足SUM_NUM个，就需要不断地更新预存理想值，直到总哈希速率不再变化或者提交个数已经足够多；
        // 
        // 令难度为当前难度diff,：e      = 4G / Vh, T1 = e * diff.

        // 当两次哈希速率的差值绝对值 大于 1M时，可能需要重新填充预设值
        if ( (submit_count == 2) || ((submit_count < SUM_NUM) && (abs(global_hashrate - hashrate) > 1000*1000*1)) )
        {
            hashrate = global_hashrate;

            if (hashrate != 0)
            {
                double ev = (((double)(4*1024*1024)) / (double)hashrate ) * (double)1024;
                double tmp = diff * ev;

                sum_time = 0;
                sum_diff = 0;

                // 必须对所有值（而不是部分）进行重新设置，并重新求和
                for (int i = 0; i < SUM_NUM*2; i=i+2)
                {
                    sum_buf[i] = tmp;
                    sum_buf[i+1] = diff;
                    
                    sum_time += sum_buf[i];
                    sum_diff += sum_buf[i+1];
                }
            }
        }
        
        if ((submit_count + SUM_NUM - 1) <= SUM_NUM)
        {
            // 累加新的值
            sum_time += time;
            sum_diff += diff;

            // 覆盖写入新值
            sum_buf[wrx++] = time;
            sum_buf[wrx++] = diff;
            if (wrx >= SUM_NUM*2)
            {
                wrx = 0;
            }   
            
            return (sum_time/sum_diff); // sum_time/sum_diff = (sum_time/n) / (sum_diff/n)
        }
        else
        {
            // 先减去最早的值，再累加最新的值，
            sum_time -= sum_buf[rdx++];
            sum_diff -= sum_buf[rdx++];
            if (rdx >= SUM_NUM*2)
            {
                rdx = 0;
            } 
            sum_time += time;
            sum_diff += diff;

            // 覆盖写入最新值
            sum_buf[wrx++] = time;
            sum_buf[wrx++] = diff;
            if (wrx >= SUM_NUM*2)
            {
                wrx = 0;
            } 

            return (sum_time/sum_diff); // sum_time/sum_diff = (sum_time/n) / (sum_diff/n)
        }
    }
    else
    {
        rdx = 0;
        wrx = 0;

        sum_time = 0;
        sum_diff = 0;  
    }

    return 0.;
}

static int share_result(int res_id, int result, struct work *work, const char *reason)
{
    int opt_algo = miner_cb[res_id].opt_algo;

	const char *flag;
	char suppl[32] = { 0 };
	char s[345];
	double hashrate;
	double sharediff = work ? work->sharediff : stratum_v[res_id].sharediff;
	int i;

	hashrate = calc_all_hashrate();

	pthread_mutex_lock(&stats_lock);
	result ? accepted_count++ : rejected_count++;
	pthread_mutex_unlock(&stats_lock);

	global_hashrate = (uint64_t) hashrate;

	if (!net_diff || sharediff < net_diff) {
		flag = use_colors ?
			(result ? CL_GRN YES : CL_RED BOO)
		:	(result ? "(" YES ")" : "(" BOO ")");
	} else {
		solved_count++;
		flag = use_colors ?
			(result ? CL_GRN YAY : CL_RED BOO)
		:	(result ? "(" YAY ")" : "(" BOO ")");
	}

	if (opt_showdiff)
		sprintf(suppl, "diff %.3f", sharediff);
	else // accepted percent
		sprintf(suppl, "%.2f%%", 100. * accepted_count / (accepted_count + rejected_count));

	switch (opt_algo) {
	case ALGO_AXIOM:
	case ALGO_CRYPTOLIGHT:
	case ALGO_CRYPTONIGHT:
	case ALGO_PLUCK:
	case ALGO_SCRYPTJANE:
	//case ALGO_LYRA2Z:
		sprintf(s, hashrate >= 1e6 ? "%.0f" : "%.2f", hashrate);
		applog(LOG_NOTICE, "accepted: %lu/%lu (%s), %s H/s %s",
			accepted_count, accepted_count + rejected_count,
			suppl, s, flag);
		break;
	default:

		sprintf(s, hashrate >= 1e9 ? "%.0f" : "%.2f", hashrate / 1000000.0);

        if (opt_url_cnt <= 1)   // 多算法时，不支持算力统计（因为不同算法的难度系数不是一个概念） 
        {
            applog(LOG_NOTICE, "accepted: %lu/%lu (%s), %s MH/s, %s[%.3f MH/s]%s %s",
                accepted_count, accepted_count + rejected_count,
                suppl, s, CL_GRN, exp_rate, CL_WHT, flag);
        }
        else
        {
            applog(LOG_NOTICE, "%s: accepted: %lu/%lu (%s), %s MH/s, %s",
                algo_names[opt_algo], accepted_count, accepted_count + rejected_count,
                suppl, s, flag);
        }

		break;
	}

	if (reason) {
		applog(LOG_WARNING, "reject reason: %s", reason);
		if (0 && strncmp(reason, "low difficulty share", 20) == 0) {
			miner_cb[work->res_id].opt_diff_factor = (miner_cb[work->res_id].opt_diff_factor * 2.0) / 3.0;
			applog(LOG_WARNING, "factor reduced to : %0.2f", miner_cb[work->res_id].opt_diff_factor);
			return 0;
		}
	}

	double accept_ratio = (double)accepted_count / (accepted_count + rejected_count);
	if ( (accepted_count + rejected_count > 100) && (accept_ratio >= opt_accept_limit) )
	{
	    high_accept_ever = true;
	}

	if ((accepted_count + rejected_count > 100) && (accept_ratio < opt_accept_limit))
	{
		applog(LOG_ERR, "accept lower limit < %0.3f", opt_accept_limit);

		if (high_accept_ever)
		    proper_exit(ERR_DROP_ACCEPT);
		else 
		    proper_exit(ERR_LOW_ACCEPT);
	}
	return 1;
}

static bool submit_upstream_work(CURL *curl, struct work *work)
{
#define SPACE_4G_VALUE  ( (double)0x100000000 )

    char* rpc_url = miner_cb[work->res_id].rpc_url;
    char* rpc_user = miner_cb[work->res_id].rpc_user;
    char* rpc_userpass = miner_cb[work->res_id].rpc_userpass;
    struct stratum_ctx *stratum = &stratum_v[work->res_id];
    int opt_algo = miner_cb[work->res_id].opt_algo;
    bool have_gbt = miner_cb[work->res_id].have_gbt;

	json_t *val, *res, *reason;
	char s[JSON_BUF_LEN];
	int i;
	bool rc = false;

	static struct timeval tv_time_last;     // 上次提交时间
	
	struct timeval tv_time;                 // 当前（或本次）提交时间
	struct timeval tv_d;                    // 两次提交时间差

	double ev = 1.0;

	/* pass if the previous hash is not the current previous hash */
	if (opt_algo != ALGO_SIA && !submit_old && memcmp(&work->data[1], &g_work_v[work->res_id].data[1], 32)) {
		if (opt_debug)
			applog(LOG_DEBUG, "DEBUG: stale work detected, discarding");
		return true;
	}

	if (!have_stratum && allow_mininginfo && g_opt_algo != ALGO_VBLAKE) {
		struct work wheight;
		get_mininginfo(curl, &wheight);
		if (work->height && work->height <= net_blocks) {
			if (opt_debug)
				applog(LOG_WARNING, "block %u was already solved", work->height);
			return true;
		}
	}

	if (have_stratum) {
        uint32_t nonceL, nonceH;
        uint64_t nonce = 0;
        uint64_t ntime = 0;
        char ntimestr[16+1], noncestr[16+1];

		if(g_opt_algo == ALGO_VBLAKE)
		{
			snprintf(s, JSON_BUF_LEN, "{\"command\":\"MINING_SUBMIT\",\"extra_nonce\":{\"data\": %llu,\"type\":\"EXTRA_NONCE\"},\"job_id\":{\"data\":%d,\"type\":\"JOB_ID\"},\"nTime\":{\"data\":%u,\"type\":\"TIMESTAMP\"},\"nonce\":{\"data\":%u,\"type\":\"NONCE\"},\"request_id\":{\"data\":%u,\"type\":\"REQUEST_ID\"}}", work->extraNonce, work->jobId, work->timestamp, work->nonce, g_request_id++);
			//share_result(work->res_id, 1, work, NULL);
		}
		else if (jsonrpc_2) {
			uchar hash[32];

			bin2hex(noncestr, (const unsigned char *)work->data + 39, 4);
			switch(opt_algo) {
			case ALGO_CRYPTOLIGHT:
				cryptolight_hash(hash, work->data, 76);
				break;
			case ALGO_CRYPTONIGHT:
				cryptonight_hash(hash, work->data, 76);
			default:
				break;
			}
			char *hashhex = abin2hex(hash, 32);
			snprintf(s, JSON_BUF_LEN,
					"{\"method\": \"submit\", \"params\": {\"id\": \"%s\", \"job_id\": \"%s\", \"nonce\": \"%s\", \"result\": \"%s\"}, \"id\":4}\r\n",
					rpc2_id, work->job_id, noncestr, hashhex);
			free(hashhex);
		} else {
			char *xnonce2str;

			switch (opt_algo) {
			case ALGO_DECRED:
				/* reversed */
				be32enc(&ntime, work->data[34]);
				be32enc(&nonce, work->data[35]);
				break;
			case ALGO_LBRY:
				le32enc(&ntime, work->data[25]);
				le32enc(&nonce, work->data[27]);
				break;
			case ALGO_DROP:
			case ALGO_NEOSCRYPT:
			case ALGO_ZR5:
				/* reversed */
				be32enc(&ntime, work->data[17]);
				be32enc(&nonce, work->data[19]);
				break;
			case ALGO_SIA:
				/* reversed */
				//be32enc(&ntime, work->data[10]);
				ntime = work->data[10];
                //nonceL = work->data[8];
				nonceL = work->data[8];
				nonceH = work->data[9];
                nonce = ((uint64_t)nonceH << 32) + nonceL;
				break;
			default:
				le32enc(&ntime, work->data[17]);
				le32enc(&nonce, work->data[19]);
			}

            if (opt_algo == ALGO_SIA) {
			    bin2hex(ntimestr, (const unsigned char *)(&ntime), 8);
			    bin2hex(noncestr, (const unsigned char *)(&nonce), 8);
            } else {
			    bin2hex(ntimestr, (const unsigned char *)(&ntime), 4);
			    bin2hex(noncestr, (const unsigned char *)(&nonce), 4);
            }

			if (opt_algo == ALGO_DECRED) {
				xnonce2str = abin2hex((unsigned char*)(&work->data[36]), stratum_v[work->res_id].xnonce1_size);
			} else if (opt_algo == ALGO_SIA) {
				//uint16_t high_nonce = swab32(work->data[9]) >> 16;
				//xnonce2str = abin2hex((unsigned char*)(&high_nonce), 2);
				xnonce2str = abin2hex(work->xnonce2, work->xnonce2_len);
			} else {
				xnonce2str = abin2hex(work->xnonce2, work->xnonce2_len);
			}
			snprintf(s, JSON_BUF_LEN,
					"{\"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"], \"id\":4}",
					rpc_user, work->job_id, xnonce2str, ntimestr, noncestr);
            //applog(LOG_WARNING, "%s", s);
			free(xnonce2str);
		}

		// store to keep/display solved blocs (work struct not linked on accept notification)
		stratum->sharediff = work->sharediff;

		// 当收到矿池应答yes时，将使用这个难度用来统计真实算力（已经不再使用）
		stratum->targetdiff = work->targetdiff;

        //
        // 真实算力将在提交Nonce时进行统计: 
        // 
        // 根据矿池应答yes的速率来统计实际的算力，方法如下：
        // 
        // 已知如果难度为 1.0，那么Target的第29字节至第22字节的值为 m = C = 0x0000ffff00000000
        // 即 targetdiff = 0x0000ffff00000000 / m = 1.0 （当矿池调整难度时m的值会变化，参见函数target_to_diff）
        // 
        // 设搜索空间 S =      C * 4G, 则成功哈希到一个目标nonce的概率为 P1 = m / S = m / (C * 4G). 
        // 即在统计上（或期望值上）芯片上报一个nonce的哈希次数为 N = 1/P1 = (C * 4G) / m
        // 设哈希到一个目标nonce的统计（或期望）时间为 T1，则 Vh = N / T1 = (C * 4G) / (m * T1)
        // 
        // 设目标难度为 Df = C / m, 即 m = C / Df, 则 Vh = (C * 4G) / (m * T1) = (Df * 4G) / T1
        // 
        // 设常系数 e = T1 / Df，则 Vh = 4G / e.
        // 
        // 程序设计：对矿池应答的每个yes的时间以及其对应的目标难度分别求出平均值，并算出常系数 e ，从而算出统计算力
        // 
        submit_count++;
            
        if (1) 
        {
		    pthread_mutex_lock(&stats_lock);
    		clr_interval = 0;
    		pthread_mutex_unlock(&stats_lock);
		    
		    if (submit_count <= 1)    // first submit
		    {
		        gettimeofday(&tv_time_last, NULL);
		    }
		    else
		    {
		        gettimeofday(&tv_time, NULL);
		        timeval_subtract(&tv_d, &tv_time, &tv_time_last);
		        tv_time_last = tv_time;
		        
                double tm = (double)(tv_d.tv_sec) + (double)(tv_d.tv_usec) / 1000000.0;        
		        ev = average_ev(tm, work->targetdiff);
			    applog(LOG_DEBUG, "calc epsilon = Time/diff = %.6lf (hashrate = 4G/epsilon)", ev);

		        pthread_mutex_lock(&stats_lock);
            	exp_rate = (SPACE_4G_VALUE/ev)/1000000.0;
            	pthread_mutex_unlock(&stats_lock);
		    }
		}

		//applog(LOG_INFO, "stratum submit debug %s", s);
		if (unlikely(!stratum_send_line(stratum, s))) {
			applog(LOG_ERR, "submit_upstream_work stratum_send_line failed");
			goto out;
		}

	} else if (work->txs) { /* gbt */

		char data_str[2 * sizeof(work->data) + 1];
		char *req;

		for (i = 0; i < ARRAY_SIZE(work->data); i++)
			be32enc(work->data + i, work->data[i]);
		bin2hex(data_str, (unsigned char *)work->data, 80);
		if (work->workid) {
			char *params;
			val = json_object();
			json_object_set_new(val, "workid", json_string(work->workid));
			params = json_dumps(val, 0);
			json_decref(val);
			req = (char*) malloc(128 + 2 * 80 + strlen(work->txs) + strlen(params));
			sprintf(req,
				"{\"method\": \"submitblock\", \"params\": [\"%s%s\", %s], \"id\":4}\r\n",
				data_str, work->txs, params);
			free(params);
		} else {
			req = (char*) malloc(128 + 2 * 80 + strlen(work->txs));
			sprintf(req,
				"{\"method\": \"submitblock\", \"params\": [\"%s%s\"], \"id\":4}\r\n",
				data_str, work->txs);
		}

		val = json_rpc_call(have_gbt, curl, rpc_url, rpc_userpass, req, NULL, 0);
		free(req);
		if (unlikely(!val)) {
			applog(LOG_ERR, "submit_upstream_work json_rpc_call failed");
			goto out;
		}

		res = json_object_get(val, "result");
		if (json_is_object(res)) {
			char *res_str;
			bool sumres = false;
			void *iter = json_object_iter(res);
			while (iter) {
				if (json_is_null(json_object_iter_value(iter))) {
					sumres = true;
					break;
				}
				iter = json_object_iter_next(res, iter);
			}
			res_str = json_dumps(res, 0);
			share_result(work->res_id, sumres, work, res_str);
			free(res_str);
		} else
			share_result(work->res_id, json_is_null(res), work, json_string_value(res));

		json_decref(val);

	} else {

		char* gw_str = NULL;
		int data_size = 128;
		int adata_sz;

		if (jsonrpc_2) {
			char noncestr[9];
			uchar hash[32];
			char *hashhex;

			bin2hex(noncestr, (const unsigned char *)work->data + 39, 4);

			switch(opt_algo) {
			case ALGO_CRYPTOLIGHT:
				cryptolight_hash(hash, work->data, 76);
				break;
			case ALGO_CRYPTONIGHT:
				cryptonight_hash(hash, work->data, 76);
			default:
				break;
			}
			hashhex = abin2hex(&hash[0], 32);
			snprintf(s, JSON_BUF_LEN,
					"{\"method\": \"submit\", \"params\": "
						"{\"id\": \"%s\", \"job_id\": \"%s\", \"nonce\": \"%s\", \"result\": \"%s\"},"
					"\"id\":4}\r\n",
					rpc2_id, work->job_id, noncestr, hashhex);
			free(hashhex);

			/* issue JSON-RPC request */
			val = json_rpc2_call(have_gbt, curl, rpc_url, rpc_userpass, s, NULL, 0);
			if (unlikely(!val)) {
				applog(LOG_ERR, "submit_upstream_work json_rpc_call failed");
				goto out;
			}
			res = json_object_get(val, "result");
			json_t *status = json_object_get(res, "status");
			bool valid = !strcmp(status ? json_string_value(status) : "", "OK");
			if (valid)
				share_result(work->res_id, valid, work, NULL);
			else {
				json_t *err = json_object_get(res, "error");
				const char *sreason = json_string_value(json_object_get(err, "message"));
				share_result(work->res_id, valid, work, sreason);
				if (!strcasecmp("Invalid job id", sreason)) {
					work_free(work);
					work_copy(work, &g_work_v[work->res_id]);
					g_work_time_v[work->res_id] = 0;
					clear_all_miners(opt_algo);
				}
			}
			json_decref(val);
			return true;

		} else if (opt_algo == ALGO_DROP || opt_algo == ALGO_NEOSCRYPT || opt_algo == ALGO_ZR5) {
			/* different data size */
			data_size = 80;
		} else if (opt_algo == ALGO_DECRED) {
			/* bigger data size : 180 + terminal hash ending */
			data_size = 192;
        } else if (opt_algo == ALGO_PHI2 && use_roots) {
            data_size = 144; 
        }
		adata_sz = data_size / sizeof(uint32_t);
		if (opt_algo == ALGO_DECRED) adata_sz = 180 / 4; // dont touch the end tag

		/* build hex string */
		for (i = 0; i < adata_sz; i++)
			le32enc(&work->data[i], work->data[i]);

		gw_str = abin2hex((uchar*)work->data, data_size);

		if (unlikely(!gw_str)) {
			applog(LOG_ERR, "submit_upstream_work OOM");
			return false;
		}

		//applog(LOG_WARNING, gw_str);

		/* build JSON-RPC request */
		snprintf(s, JSON_BUF_LEN,
			"{\"method\": \"getwork\", \"params\": [\"%s\"], \"id\":4}\r\n", gw_str);
		free(gw_str);

		/* issue JSON-RPC request */
		val = json_rpc_call(have_gbt, curl, rpc_url, rpc_userpass, s, NULL, 0);
		if (unlikely(!val)) {
			applog(LOG_ERR, "submit_upstream_work json_rpc_call failed");
			goto out;
		}
		res = json_object_get(val, "result");
		reason = json_object_get(val, "reject-reason");
		share_result(work->res_id, json_is_true(res), work, reason ? json_string_value(reason) : NULL);

		json_decref(val);
	}

	rc = true;

out:
	return rc;
}

static const char *getwork_req =
	"{\"method\": \"getwork\", \"params\": [], \"id\":0}\r\n";

#define GBT_CAPABILITIES "[\"coinbasetxn\", \"coinbasevalue\", \"longpoll\", \"workid\"]"

static const char *gbt_req =
	"{\"method\": \"getblocktemplate\", \"params\": [{\"capabilities\": "
	GBT_CAPABILITIES "}], \"id\":0}\r\n";
static const char *gbt_lp_req =
	"{\"method\": \"getblocktemplate\", \"params\": [{\"capabilities\": "
	GBT_CAPABILITIES ", \"longpollid\": \"%s\"}], \"id\":0}\r\n";

static bool get_upstream_work(CURL *curl, struct work *work)
{
    char *rpc_url = miner_cb[work->res_id].rpc_url;
    char *rpc_userpass = miner_cb[work->res_id].rpc_userpass;
    bool have_gbt = miner_cb[work->res_id].have_gbt;

	json_t *val;
	int err;
	bool rc;
	struct timeval tv_start, tv_end, diff;

start:
	gettimeofday(&tv_start, NULL);

	if (jsonrpc_2) {
		char s[128];
		snprintf(s, 128, "{\"method\": \"getjob\", \"params\": {\"id\": \"%s\"}, \"id\":1}\r\n", rpc2_id);
		val = json_rpc2_call(have_gbt, curl, rpc_url, rpc_userpass, s, NULL, 0);
	} else {
		val = json_rpc_call(have_gbt, curl, rpc_url, rpc_userpass,
		                    have_gbt ? gbt_req : getwork_req,
		                    &err, have_gbt ? JSON_RPC_QUIET_404 : 0);
	}
	gettimeofday(&tv_end, NULL);

	if (have_stratum) {
		if (val)
			json_decref(val);
		return true;
	}

	if (!have_gbt && !allow_getwork) {
		applog(LOG_ERR, "No usable protocol");
		if (val)
			json_decref(val);
		return false;
	}

	if (have_gbt && allow_getwork && !val && err == CURLE_OK) {
		applog(LOG_NOTICE, "getblocktemplate failed, falling back to getwork");
		have_gbt = false;
		goto start;
	}

	if (!val)
		return false;

	if (have_gbt) {
		rc = gbt_work_decode(json_object_get(val, "result"), work);
		if (!have_gbt) {
			json_decref(val);
			goto start;
		}
	} else {
		rc = work_decode(json_object_get(val, "result"), work);
	}

	if (opt_protocol && rc) {
		timeval_subtract(&diff, &tv_end, &tv_start);
		applog(LOG_DEBUG, "got new work in %.2f ms",
		       (1000.0 * diff.tv_sec) + (0.001 * diff.tv_usec));
	}

	json_decref(val);

	// store work height in solo
	get_mininginfo(curl, work);

	return rc;
}

static void workio_cmd_free(struct workio_cmd *wc)
{
	if (!wc)
		return;

	switch (wc->cmd) {
	case WC_SUBMIT_WORK:
		work_free(wc->u.work);
		free(wc->u.work);
		break;
	default: /* do nothing */
		break;
	}

	memset(wc, 0, sizeof(*wc)); /* poison */
	free(wc);
}

static bool workio_get_work(struct workio_cmd *wc, CURL *curl)
{
	struct work *ret_work;
	int failures = 0;

	ret_work = (struct work*) calloc(1, sizeof(*ret_work));
	if (!ret_work)
		return false;

	/* obtain new work from bitcoin via JSON-RPC */
	while (!get_upstream_work(curl, ret_work)) {
		if (unlikely((opt_retries >= 0) && (++failures > opt_retries))) {
			applog(LOG_ERR, "json_rpc_call failed, terminating workio thread");
			free(ret_work);
			return false;
		}

		/* pause, then restart work-request loop */
		applog(LOG_ERR, "json_rpc_call failed, retry after %d seconds",
			opt_fail_pause);
		sleep(opt_fail_pause);
	}

	/* send work to requesting thread */
	if (!tq_push(wc->thr->q, ret_work))
		free(ret_work);

	return true;
}

static bool workio_submit_work(struct workio_cmd *wc, CURL *curl)
{
	int failures = 0;
    struct miner_chip* miner_chip = (struct miner_chip *)(wc->thr);

	/* submit solution to bitcoin via JSON-RPC */
	while (!submit_upstream_work(curl, wc->u.work)) {
		if (unlikely((opt_retries >= 0) && (++failures > opt_retries))) {
			applog(LOG_ERR, "...terminating workio thread");
			return false;
		}

		/* pause, then restart work-request loop */
		if (!opt_benchmark)
			applog(LOG_ERR, "...retry after %d seconds", opt_fail_pause);
		sleep(opt_fail_pause);
	}

	return true;
}

bool rpc2_login(CURL *curl)
{
    // rpc2 not use
    char *rpc_user = miner_cb[0].rpc_user;
    char *rpc_pass = miner_cb[0].rpc_pass;
    char *rpc_userpass =miner_cb[0].rpc_userpass;
    char *rpc_url = miner_cb[0].rpc_url;

	json_t *val;
	bool rc = false;
	struct timeval tv_start, tv_end, diff;
	char s[JSON_BUF_LEN];
    bool have_gbt = miner_cb[0].have_gbt;

	if (!jsonrpc_2)
		return false;

	snprintf(s, JSON_BUF_LEN, "{\"method\": \"login\", \"params\": {"
		"\"login\": \"%s\", \"pass\": \"%s\", \"agent\": \"%s\"}, \"id\": 1}",
		rpc_user, rpc_pass, USER_AGENT);

	gettimeofday(&tv_start, NULL);
	val = json_rpc_call(have_gbt, curl, rpc_url, rpc_userpass, s, NULL, 0);
	gettimeofday(&tv_end, NULL);

	if (!val)
		goto end;

//	applog(LOG_DEBUG, "JSON value: %s", json_dumps(val, 0));

	rc = rpc2_login_decode(val);

	json_t *result = json_object_get(val, "result");

	if (!result)
		goto end;

	json_t *job = json_object_get(result, "job");
	if (!rpc2_job_decode(job, &g_work_v[0])) {
		goto end;
	}

	if (opt_debug && rc) {
		timeval_subtract(&diff, &tv_end, &tv_start);
		applog(LOG_DEBUG, "DEBUG: authenticated in %d ms",
				diff.tv_sec * 1000 + diff.tv_usec / 1000);
	}

	json_decref(val);
end:
	return rc;
}

bool rpc2_workio_login(CURL *curl)
{
	int failures = 0;
	if (opt_benchmark)
		return true;
	/* submit solution to bitcoin via JSON-RPC */
	pthread_mutex_lock(&rpc2_login_lock);
	while (!rpc2_login(curl)) {
		if (unlikely((opt_retries >= 0) && (++failures > opt_retries))) {
			applog(LOG_ERR, "...terminating workio thread");
			pthread_mutex_unlock(&rpc2_login_lock);
			return false;
		}

		/* pause, then restart work-request loop */
		if (!opt_benchmark)
			applog(LOG_ERR, "...retry after %d seconds", opt_fail_pause);
		sleep(opt_fail_pause);
		pthread_mutex_unlock(&rpc2_login_lock);
		pthread_mutex_lock(&rpc2_login_lock);
	}
	pthread_mutex_unlock(&rpc2_login_lock);

	return true;
}

static void *workio_thread(void *userdata)
{
	struct thr_info *mythr = (struct thr_info *) userdata;
	CURL *curl;
	bool ok = true;
    //char *rpcurl = &miner_cb[mythr->id].rpc_url;

	curl = curl_easy_init();
	if (unlikely(!curl)) {
		applog(LOG_ERR, "CURL initialization failed");
		return NULL;
	}

	if(jsonrpc_2 && !have_stratum) {
		ok = rpc2_workio_login(curl);
	}

	while (ok) {
		struct workio_cmd *wc;

		/* wait for workio_cmd sent to us, on our queue */
		wc = (struct workio_cmd *) tq_pop(mythr->q, NULL);
		if (!wc) {
			ok = false;
			break;
		}

		/* process workio_cmd */
		switch (wc->cmd) {
		case WC_GET_WORK:
			ok = workio_get_work(wc, curl);
			break;
		case WC_SUBMIT_WORK:
			ok = workio_submit_work(wc, curl);
			break;

		default:		/* should never happen */
			ok = false;
			break;
		}

		workio_cmd_free(wc);
	}

	tq_freeze(mythr->q);
	curl_easy_cleanup(curl);

	return NULL;
}

static bool get_work(struct thr_info *thr, struct work *work)
{
    int opt_algo = miner_cb[work->res_id].opt_algo;

	struct workio_cmd *wc;
	struct work *work_heap;

	if (opt_benchmark) {
		uint32_t ts = (uint32_t) time(NULL);
		for (int n=0; n<74; n++) ((char*)work->data)[n] = n;
		//memset(work->data, 0x55, 76);
		work->data[17] = swab32(ts);
		memset(work->data + 19, 0x00, 52);
		if (opt_algo == ALGO_DECRED) {
			memset(&work->data[35], 0x00, 52);
		} else {
			work->data[20] = 0x80000000;
			work->data[31] = 0x00000280;
		}
		memset(work->target, 0x00, sizeof(work->target));
		return true;
	}

	/* fill out work request message */
	wc = (struct workio_cmd *) calloc(1, sizeof(*wc));
	if (!wc)
		return false;

	wc->cmd = WC_GET_WORK;
	wc->thr = thr;

	/* send work request to workio thread */
	if (!tq_push(thr_info[work->res_id].q, wc)) {
		workio_cmd_free(wc);
		return false;
	}

	/* wait for response, a unit of work */
	work_heap = (struct work*) tq_pop(thr->q, NULL);
	if (!work_heap)
		return false;

	/* copy returned work into storage provided by caller */
	memcpy(work, work_heap, sizeof(*work));
	free(work_heap);

	return true;
}

static bool submit_work(struct thr_info *thr, const struct work *work_in)
{
	struct workio_cmd *wc;

	///debug result
	//printf("SubMitWork_Check:\n");
	//for(int i=0;i<3;i++)
	//{
	//	for (int j = 0; j < 16; j++)
	//	{
	//		printf("%08x ", work_in->data[i*16 + j]);
	//	}
	//	printf("\n");
	//}

	//printf("sub_resnonce	=%08x\n",work_in->resnonce);
//printf("sub_height	=%08x\n",work_in->height);
//printf("sub_workid	=%2d\n" ,work_in->workid[0]);
//printf("sub_job_id	=%2d\n" ,work_in->job_id[0]);
//printf("sub_xlen	=%ld\n"  ,work_in->xnonce2_len);
//for(int i=0;i<8;i++)
//printf("sub_xlen2dat[%2d]=%08x\n",i,work_in->xnonce2[i]);

	/* fill out work request message */
	wc = (struct workio_cmd *) calloc(1, sizeof(*wc));
	if (!wc)
		return false;

	wc->u.work = (struct work*) malloc(sizeof(*work_in));
	if (!wc->u.work)
		goto err_out;

	wc->cmd = WC_SUBMIT_WORK;
	wc->thr = thr;
	work_copy(wc->u.work, work_in);

	/* send solution to workio thread */
	if (!tq_push(thr_info[work_in->res_id].q, wc))
		goto err_out;

	return true;

err_out:
	workio_cmd_free(wc);
	return false;
}

// vblake gen work
static void longToByteArray(uint64_t input, uint8_t output[8])
{
	output[0] =             (uint8_t) ((input & 0xFF00000000000000l) >> 56);
	output[1] =             (uint8_t) ((input & 0x00FF000000000000l) >> 48);
	output[2] =             (uint8_t) ((input & 0x0000FF0000000000l) >> 40);
	output[3] =             (uint8_t) ((input & 0x000000FF00000000l) >> 32);
	output[4] =             (uint8_t) ((input & 0x00000000FF000000l) >> 24);
	output[5] =             (uint8_t) ((input & 0x0000000000FF0000l) >> 16);
	output[6] =             (uint8_t) ((input & 0x000000000000FF00l) >> 8);
	output[7] =             (uint8_t) ((input & 0x00000000000000FFl));
}

static void concatByte(uint8_t* des, uint8_t* first, int firstlen, uint8_t* second, int secondlen)
{
	memcpy(des, first, firstlen);
	memcpy(des+firstlen, second, secondlen);
}

void Sha256_Hash(uint8_t hash[32], uint8_t* data, int len)
{
	SHA256_CTX ctx; 
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, data, len);
	SHA256_Final(hash, &ctx);
}

void PrintHex(uint8_t* data, int len, const char* format)
{
	printf("%s: ", format);
	for(int i = 0; i < len; i++)
	{
		printf("%02x", data[i]);
	}
	printf("\n");
}

void BuildMerkleRoot(uint8_t hash[32], uint8_t* first, int first_len, uint8_t* second, int second_len)
{
	uint8_t* concat = NULL;
	concat = (uint8_t*)malloc(first_len+second_len);
	concatByte(concat, first, first_len, second, second_len);
	Sha256_Hash(hash, concat, first_len+second_len);
	if(concat)
		free(concat);
}

static void vBlake_gen_MerkelRoot_withExtra(uint8_t trimmedContentMerkleRoot[24], uint8_t intermediateMetapackageHashByte[32], uint8_t popTxMerkleRootByte[32], uint8_t normalTxMerkleRootByte[32], uint64_t extraNonce)
{
        uint8_t extraNonceByte[8];
        uint8_t txMerkleRoot[32];
        uint8_t merkleRoot[32];
        uint8_t blockContentMerkleRootWithExtraNonce[32] = {0};
        longToByteArray(extraNonce, extraNonceByte);
        BuildMerkleRoot(blockContentMerkleRootWithExtraNonce, intermediateMetapackageHashByte, 32, extraNonceByte, 8);
        //PrintHex(blockContentMerkleRootWithExtraNonce, 32); 
        BuildMerkleRoot(txMerkleRoot, popTxMerkleRootByte, 32, normalTxMerkleRootByte, 32);
        BuildMerkleRoot(merkleRoot, blockContentMerkleRootWithExtraNonce, 32, txMerkleRoot, 32);
        memcpy(trimmedContentMerkleRoot, merkleRoot, 24);
        //PrintHex(trimmedContentMerkleRoot, 24, "final");
}

static void vBlake_gen_hashdata(struct stratum_ctx *sctx, struct work *work)
{
	uint8_t merkleRoot[24] = {0};

	for (int i = 0; i < 4; i++) {
		work->poolData[i] = (sctx->job.blockHeight >> ((3 - i) * 8));
	}

	for (int i = 0; i < 2; i++) {
		work->poolData[i + 4] = (sctx->job.blockVersion >> ((1 - i) * 8));
	}

	for (int i = 0; i < 4; i++) {
		work->poolData[52 + i] = (work->timestamp >> ((3 - i) * 8));
	}

	for (int i = 0; i < 4; i++) {
		work->poolData[56 + i] = (sctx->job.encodedDiff >> ((3 - i) * 8));
	}

	memcpy(work->poolData+6, sctx->job.prevBlockHash+12, 12);
	memcpy(work->poolData+18, sctx->job.secBlockHash+15, 9);
	memcpy(work->poolData+27, sctx->job.thdBlockHash+15, 9);

	vBlake_gen_MerkelRoot_withExtra(merkleRoot, sctx->job.interMetaHash, sctx->job.popTxRoot, sctx->job.normalTxRoot, work->extraNonce);
	memcpy(work->poolData+36, merkleRoot, 16);
}

static void vBlake_getNextExtraNonce(struct stratum_ctx *sctx, struct work *work)
{
	if(sctx->job.extraNonce + sctx->job.extraNonceOffset + 1 >= sctx->job.extraNonceEnd)
	{
		sctx->job.extraNonceOffset = 0;
		sctx->job.timestamp_offset++;
	}
	work->extraNonce = sctx->job.extraNonce + sctx->job.extraNonceOffset++;
	work->timestamp = sctx->job.timestamp + sctx->job.timestamp_offset;
	applog(LOG_INFO, "vblake getNextExtraNonce timestamp: %u ExtraNonce: %llu", work->timestamp, work->extraNonce);
}

static void stratum_gen_work(struct stratum_ctx *sctx, struct work *work)
{
    int opt_algo = miner_cb[work->res_id].opt_algo;

	uint32_t extraheader[32] = { 0 };
	uchar merkle_root[65] = { 0 };
	uchar merkle_root0[64] = { 0 };
	int i, headersize = 0;
    char *short_url = miner_cb[work->res_id].short_url;

	uint8_t hash_buf[256] = { 0 };

	if(g_opt_algo == ALGO_VBLAKE)
	{
		//applog(LOG_INFO, "vblake gen work");
		pthread_mutex_lock(&sctx->work_lock);
		work->jobId = sctx->job.jobId;
		vBlake_getNextExtraNonce(sctx, work);
		memcpy(work->target_u8, sctx->job.target_u8, 32);
		// custom diff
		work->target_u8[4] /= miner_cb[opt_url_cnt-1].opt_diff_factor;
		vBlake_gen_hashdata(sctx, work);
		pthread_mutex_unlock(&sctx->work_lock);
		return;
    } else if (g_opt_algo == ALGO_TRIGG) {
        // broot(32) + nblock(4) + rand(4)
		pthread_mutex_lock(&sctx->work_lock);
		work->nblock = sctx->nblock;
		work->tdiff = sctx->tdiff;
        memset(work->target, 0xff, sizeof(work->target));
        work->target[7] = 0;    // always 0 for getwork
		memcpy(work->data, sctx->broot, 32);
		memcpy(work->data+8, &sctx->nblock, 4);
        work->nonce_rand++;     // random ??
		memcpy(work->data+9, &work->nonce_rand, 4);
		pthread_mutex_unlock(&sctx->work_lock);
		return;
    }

	pthread_mutex_lock(&sctx->work_lock);

	if (jsonrpc_2) {
		work_free(work);
		work_copy(work, &sctx->work);
		pthread_mutex_unlock(&sctx->work_lock);
	} else {
		free(work->job_id);
		work->job_id = strdup(sctx->job.job_id);
		work->xnonce2_len = sctx->xnonce2_size;
		work->xnonce2 = (uchar*) realloc(work->xnonce2, sctx->xnonce2_size);
		memcpy(work->xnonce2, sctx->job.xnonce2, sctx->xnonce2_size);

		/* Generate merkle root */
		switch (opt_algo) {
			case ALGO_DECRED:
				// getwork over stratum, getwork merkle + header passed in coinb1
				memcpy(merkle_root, sctx->job.coinbase, 32);
				headersize = min((int)sctx->job.coinbase_size - 32, sizeof(extraheader));
				memcpy(extraheader, &sctx->job.coinbase[32], headersize);
				break;
			case ALGO_HEAVY:
				heavyhash(merkle_root, sctx->job.coinbase, (int)sctx->job.coinbase_size);
				break;
			case ALGO_GROESTL:
			case ALGO_KECCAK:
			case ALGO_X16M:
			case ALGO_BLAKECOIN:
				SHA256(sctx->job.coinbase, (int) sctx->job.coinbase_size, merkle_root);
				break;
			case ALGO_SIA:
				// getwork over stratum, getwork merkle + header passed in coinb1
				//memcpy(merkle_root, sctx->job.coinbase, 32);
				//headersize = min((int)sctx->job.coinbase_size - 32, sizeof(extraheader));
				//memcpy(extraheader, &sctx->job.coinbase[32], headersize);
                hash_buf[0] = 0x00;
                memcpy(hash_buf+1, sctx->job.coinbase, sctx->job.coinbase_size);
                blake2b_hash_ex(merkle_root+1+32, hash_buf, sctx->job.coinbase_size + 1);
                headersize = 0;
				break;
			case ALGO_KECCAKC:
				SHA256(sctx->job.coinbase, (int) sctx->job.coinbase_size, merkle_root0);
				SHA256(merkle_root0, 32, merkle_root);
			default:
				sha256d(merkle_root, sctx->job.coinbase, (int) sctx->job.coinbase_size);
		}

		if (!headersize)
            for (i = 0; i < sctx->job.merkle_count; i++) {
                if (opt_algo == ALGO_HEAVY) {
                    memcpy(merkle_root + 32, sctx->job.merkle[i], 32);
                    heavyhash(merkle_root, merkle_root, 64);
                } else if (opt_algo == ALGO_SIA) {
                    merkle_root[0] = 0x01;
                    memcpy(merkle_root + 1, sctx->job.merkle[i], 32);
                    blake2b_hash_ex(merkle_root+1+32, merkle_root, 65);
                } else {
                    memcpy(merkle_root + 32, sctx->job.merkle[i], 32);
                    sha256d(merkle_root, merkle_root, 64);
                }
            }
        if (opt_algo == ALGO_SIA) {
            memcpy(merkle_root, merkle_root+1+32, 32);
        }

		/* Increment extranonce2 */
		for (size_t t = 0; t < sctx->xnonce2_size && !(++sctx->job.xnonce2[t]); t++)
			;

        // ALGO_SIA: highnonce/nonceH
        static uint32_t nonceH = 0;
        if (opt_algo == ALGO_SIA) {
            nonceH = 0;
        }
		/* Assemble block header */
		memset(work->data, 0, 128);
		work->data[0] = le32dec(sctx->job.version);
		for (i = 0; i < 8; i++)
			work->data[1 + i] = le32dec((uint32_t *) sctx->job.prevhash + i);
		for (i = 0; i < 8; i++)
			work->data[9 + i] = be32dec((uint32_t *) merkle_root + i);

		if (opt_algo == ALGO_DECRED) {
			uint32_t* extradata = (uint32_t*) sctx->xnonce1;
			for (i = 0; i < 8; i++) // prevhash
				work->data[1 + i] = swab32(work->data[1 + i]);
			for (i = 0; i < 8; i++) // merkle
				work->data[9 + i] = swab32(work->data[9 + i]);
			for (i = 0; i < headersize/4; i++) // header
				work->data[17 + i] = extraheader[i];
			// extradata
			for (i = 0; i < sctx->xnonce1_size/4; i++)
				work->data[36 + i] = extradata[i];
			for (i = 36 + (int) sctx->xnonce1_size/4; i < 45; i++)
				work->data[i] = 0;
			work->data[37] = (rand()*4) << 8;
			sctx->bloc_height = work->data[32];
			///printf("ex1_size=%d\n",sctx->xnonce1_size);
			////added
			work->data[44] = 0x00000005;///temp no confirm

			//applog_hex(work->data, 180);
			//applog(LOG_DEBUG, "DEBUG: work->data[36]~[44]:");
			//applog_hex(&work->data[36], 36);
		} else if (opt_algo == ALGO_LBRY) {
			for (i = 0; i < 8; i++)
				work->data[17 + i] = ((uint32_t*)sctx->job.extra)[i];
			work->data[25] = le32dec(sctx->job.ntime);
			work->data[26] = le32dec(sctx->job.nbits);
			work->data[28] = 0x80000000;
        } else if (opt_algo == ALGO_PHI2) {                  
          work->data[17] = le32dec(sctx->job.ntime);
          work->data[18] = le32dec(sctx->job.nbits);
          for (i = 0; i < 16; i++) 
                work->data[20 + i] = ((uint32_t*)sctx->job.extra)[i];
         //applog_hex(&work->data[0], 144);
		} else if (opt_algo == ALGO_SIA) {
			for (i = 0; i < 8; i++) {   // prevhash
				//work->data[i] = ((uint32_t*)sctx->job.prevhash)[7-i];
				work->data[i] = ((uint32_t*)sctx->job.prevhash)[i];
            }

			work->data[8] = 0; // nonceL
			//work->data[9] = swab32(extraheader[0]);
			//work->data[9] |= rand() & 0xF0;
            work->data[9] = nonceH;
			//work->data[10] = be32dec(sctx->job.ntime);        // ntimeL
			work->data[10] = *((uint32_t *)sctx->job.ntime);    // ntimeL
			//work->data[11] = be32dec(sctx->job.nbits);
			work->data[11] = 0;                                 // ntimeH = 0

			for (i = 0; i < 8; i++) 
				work->data[12+i] = ((uint32_t*)merkle_root)[i];
		} else {
			work->data[17] = le32dec(sctx->job.ntime);
			work->data[18] = le32dec(sctx->job.nbits);
			// required ?
			work->data[20] = 0x80000000;
			work->data[31] = 0x00000280;
		}

		if (opt_showdiff || opt_max_diff > 0.)
			calc_network_diff(work);

		if (opt_algo == ALGO_DROP || opt_algo == ALGO_NEOSCRYPT || opt_algo == ALGO_ZR5) {
			/* reversed endian */
			for (i = 0; i <= 18; i++)
				work->data[i] = swab32(work->data[i]);
		}

		pthread_mutex_unlock(&sctx->work_lock);

		if (opt_debug && opt_algo != ALGO_DECRED && opt_algo != ALGO_SIA) {
			char *xnonce2str = abin2hex(work->xnonce2, work->xnonce2_len);
			applog(LOG_BLUE, "url[%d]=%s job_id='%s' extranonce2=%s ntime=%08x",
					work->res_id, short_url, work->job_id, xnonce2str, swab32(work->data[17]));
			free(xnonce2str);
		}

		switch (opt_algo) {
			case ALGO_DROP:
			case ALGO_JHA:
			case ALGO_SCRYPT:
			case ALGO_SCRYPTJANE:
			case ALGO_NEOSCRYPT:
			case ALGO_PLUCK:
			case ALGO_YESCRYPT:
				work_set_target(work, sctx->job.diff / (65536.0 * miner_cb[work->res_id].opt_diff_factor));
				break;
			case ALGO_FRESH:
			case ALGO_DMD_GR:
			case ALGO_GROESTL:
			case ALGO_LBRY:
			case ALGO_LYRA2REV2:
			case ALGO_LYRA2REV3:
			case ALGO_TIMETRAVEL:
			case ALGO_BITCORE:
			case ALGO_XEVAN:
			case ALGO_LYRA2Z:
			case ALGO_PHI2:
			case ALGO_X16R:
			case ALGO_X16S:
				work_set_target(work, sctx->job.diff / (256.0 * miner_cb[work->res_id].opt_diff_factor));
				break;
			case ALGO_KECCAK:
			case ALGO_X16M:
			case ALGO_LYRA2:
				work_set_target(work, sctx->job.diff / (128.0 * miner_cb[work->res_id].opt_diff_factor));
				break;
			default:
				work_set_target(work, sctx->job.diff / miner_cb[work->res_id].opt_diff_factor);
		}

		if (stratum_diff != sctx->job.diff) {
			char sdiff[32] = { 0 };
			// store for api stats
			stratum_diff = sctx->job.diff;
			if (opt_showdiff && work->targetdiff != stratum_diff)
				snprintf(sdiff, 32, " (%.5f)", work->targetdiff);
			applog(LOG_INFO, "%s: Stratum difficulty set to %g%s", algo_names[opt_algo], stratum_diff, sdiff);
		}
	}
}

bool rpc2_stratum_job(struct stratum_ctx *sctx, json_t *params)
{
	bool ret = false;
	pthread_mutex_lock(&sctx->work_lock);
	ret = rpc2_job_decode(params, &sctx->work);

	if (ret) {
		work_free(&g_work_v[0]);
		work_copy(&g_work_v[0], &sctx->work);
		g_work_time_v[0] = 0;
	}

	pthread_mutex_unlock(&sctx->work_lock);

	return ret;
}

static bool wanna_mine(int thr_id)
{
	bool state = true;

	if (opt_max_temp > 0.0) {
		float temp = cpu_temp(0);
		if (temp > opt_max_temp) {
			if (!thr_id && !conditional_state[thr_id] && !opt_quiet)
				applog(LOG_INFO, "temperature too high (%.0fC), waiting...", temp);
			state = false;
		}
	}
	if (opt_max_diff > 0.0 && net_diff > opt_max_diff) {
		if (!thr_id && !conditional_state[thr_id] && !opt_quiet)
			applog(LOG_INFO, "network diff too high, waiting...");
		state = false;
	}
	if (opt_max_rate > 0.0 && net_hashrate > opt_max_rate) {
		if (!thr_id && !conditional_state[thr_id] && !opt_quiet) {
			char rate[32];
			format_hashrate(opt_max_rate, rate);
			applog(LOG_INFO, "network hashrate too high, waiting %s...", rate);
		}
		state = false;
	}
	if (thr_id < MAX_CPUS)
		conditional_state[thr_id] = (uint8_t) !state;
	return state;
}

static int gen_miner_task(struct miner_chip *miner_chip, 
        const struct chip_info* chip_info, const struct chip_config* chip_config, const struct algo_config* config, struct  miner_task*  mt)
{
    struct stratum_ctx *stratum = &stratum_v[miner_chip->res_id];
    int opt_algo = miner_cb[miner_chip->res_id].opt_algo;
	int result;

	uint32_t *nonceptr = (uint32_t*) ((char*)(mt->work.data) + config->nonce_oft);

START:

	while (!stratum->job.diff && opt_algo == ALGO_NEOSCRYPT) {
		applog(LOG_DEBUG, "Waiting for Stratum to set the job difficulty");
		sleep(1);
	}

	pthread_mutex_lock(&g_work_lock_v[miner_chip->res_id]);

    mt->work.res_id = miner_chip->res_id;
	if (mt->regen_work) {
		//applog(LOG_INFO, "DEBUG gen_miner_task stratum_gen_work");
		stratum_gen_work(stratum, &g_work_v[miner_chip->res_id]);
	}

	work_free(&mt->work);
	work_copy(&mt->work, &g_work_v[miner_chip->res_id]);
    mt->work.res_id = miner_chip->res_id;


	nonceptr = (uint32_t*) (((char*)mt->work.data) + config->nonce_oft);
	*nonceptr = 0;
	if (opt_randomize)
			nonceptr[0] += ((rand()*4) & UINT32_MAX);

	pthread_mutex_unlock(&g_work_lock_v[miner_chip->res_id]);

	if (opt_algo == ALGO_DECRED) {
		// extradata: prevent duplicates
		nonceptr[1] += 1;
		nonceptr[2] |= chip_config->index;
	} else if (opt_algo == ALGO_SIA) {
		if (have_stratum && strcmp(stratum->job.job_id, mt->work.job_id))
			goto START; // need to regen g_work..
		// extradata: prevent duplicates
		//nonceptr[1] += 0x10;
		//nonceptr[1] |= chip_config->index;
		//applog_hex(nonceptr, 8);
	}

	// prevent scans before a job is received
	// beware, some testnet (decred) are using version 0
	// no version in sia draft protocol
	if (opt_algo != ALGO_SIA && have_stratum && !mt->work.data[0] && !opt_benchmark) {
		if(g_opt_algo == ALGO_VBLAKE && mt->work.jobId == 0)
		{
			sleep(1);
			goto START;
		}
	}

	/* adjust max_nonce to meet target scan time */
	int64_t max64;
	uint32_t max_nonce;

	max64 = opt_scantime;

	max64 *= (int64_t) chip_info->hashrate;
	//max64 = (int64_t) ((double)4120000 * (double)0.005);

	if (max64 <= 0) {
		switch (config->algo) {
		case ALGO_SCRYPT:
		case ALGO_NEOSCRYPT:
			max64 = opt_scrypt_n < 16 ? 0x3ffff : 0x3fffff / opt_scrypt_n;
			if (opt_nfactor > 3)
				max64 >>= (opt_nfactor - 3);
			else if (opt_nfactor > 16)
				max64 = 0xF;
			break;
		case ALGO_AXIOM:
		case ALGO_CRYPTOLIGHT:
		case ALGO_CRYPTONIGHT:
		case ALGO_SCRYPTJANE:
			max64 = 0x40LL;
			break;
		case ALGO_DROP:
		case ALGO_PLUCK:
		case ALGO_YESCRYPT:
			max64 = 0x1ff;
			break;
		case ALGO_LYRA2:
		case ALGO_TIMETRAVEL:
		case ALGO_BITCORE:
		case ALGO_XEVAN:
			max64 = 0xffff;
			break;
		case ALGO_LYRA2REV2:
		    max64 = 0x7fffff;
		    break;
		case ALGO_LYRA2REV3:
		    max64 = 0x7fffff;
		    break;
		    
		case ALGO_C11:
		case ALGO_DMD_GR:
		case ALGO_FRESH:
		case ALGO_GROESTL:
		case ALGO_MYR_GR:
		case ALGO_SIB:
		case ALGO_VELTOR:
		case ALGO_X11EVO:
		case ALGO_X11:
		case ALGO_X13:
		case ALGO_BCD:
		case ALGO_X14:
			max64 = 0x1ffffff;
			break;
		case ALGO_LBRY:
		case ALGO_TRIBUS:
		case ALGO_X15:
		case ALGO_ZR5:
			max64 = 0x1ffffff;
			break;
		case ALGO_X17:
		    max64 = 0x3fffff;
		    break;
		case ALGO_BMW:
		case ALGO_PENTABLAKE:
			max64 = 0x3ffff;
			break;
		case ALGO_SKEIN:
		case ALGO_SKEIN2:
			max64 = 0x1ffffff;
			break;
		case ALGO_BLAKE:
		case ALGO_BLAKECOIN:
		case ALGO_DECRED:
		    if ((config->algo == ALGO_DECRED) && opt_asic_mode)
		    {
		        max64 = 0x1fffffffLL;
		    }
		    else
		    {
			    max64 = 0xffffffffLL;
			}
			break;
		case ALGO_VANILLA:
			max64 = 0x3fffffLL;
			break;
		case ALGO_BLAKE2S:
		case ALGO_BLAKE2B:
			max64 = 0x2ffffffLL;
			break;
		case ALGO_KECCAK:
			max64 = 0xfffffffLL;
			break;
		case ALGO_SIA:
		    max64 = 0x2ffffffLL;
		    break;
    	case ALGO_X16R:
    	case ALGO_X16S:
    	case ALGO_X16M:
		    max64 = 0x003fffffLL;
		    break;

    	case ALGO_SKUNK:
			max64 = 0x7fffffLL;
		    break;
    	case ALGO_TRIGG:
			max64 = 0x3fffffffLL;
		    break;

        case ALGO_LYRA2Z:
            if ((chip_model == MODEL_7K420T_40A) || (chip_model == MODEL_7K355T_40A)) {
                max64 = 40 * 0x1fffffLL;
            } else {
                max64 = 0x1fffffLL;
            }
            break;
        //case ALGO_PHI2:
		//	max64 = 500000UL;
        //    break;

		default:
			max64 = 0x1fffffLL;
			break;
		}
	}

	if ((*nonceptr) + max64 > config->end_nonce)
		max_nonce = config->end_nonce;
	else
		max_nonce = (*nonceptr) + (uint32_t) max64;

    uint8_t chip_index;
    if (opt_asic_mode)
    {
        switch (config->algo)
        {
        // middle 1: lbry(sha256 + ...) + skein(skein512 + sha256)	
    	case ALGO_LBRY:
    		result = scanhash_lbry(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_SKEIN:
    		result = scanhash_skein(chip_config->addr, chip_config, max_nonce, mt);
    		break;

    	// middle 2: myr-groestl + groestl	
    	case ALGO_MYR_GR:
    		result = scanhash_myriad(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_GROESTL:
    		result = scanhash_groestl(chip_config->addr, chip_config, max_nonce, mt);
    		break;

        // middle 3: keccak (keccak256)
    	case ALGO_KECCAK:
    	case ALGO_KECCAKC:
    		result = scanhash_keccak(chip_config->addr, chip_config, max_nonce, mt);
    		break;

        // middle 4: blake (四合一)
    	case ALGO_DECRED:
    	    result = scanhash_decred(chip_config->addr, chip_config, max_nonce, mt);
    	    break;
    	case ALGO_BLAKE2S:
    	    result = scanhash_blake2s(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_SIA:      // sia
    	    result = fpga_scanhash_sia(chip_config->addr, chip_config, max_nonce, mt);
    	    break;
    	case ALGO_BLAKE2B:  // bcx
    		result = fpga_scanhash_blake2b(chip_config->addr, chip_config, max_nonce, mt);
    		break;

    	// middle 5: lyra2rev2
    	case ALGO_LYRA2REV2:
    	    result = fpga_scanhash_lyra2rev2(chip_config->addr, chip_config, max_nonce, mt);
    	    break;

    	// middle 6: comb
    	case ALGO_NIST5:
    	case ALGO_TRIBUS: 
    	case ALGO_X13: 
    	case ALGO_BCD: 
    	case ALGO_X17:
    	case ALGO_SIB:
    	case ALGO_QUBIT:
    	case ALGO_QUARK:
    	case ALGO_X11:
    	case ALGO_PHI2:
    	case ALGO_X16R:
    	case ALGO_X16S:
    	case ALGO_X16M:
    	    chip_index = (chip_config->addr - 1) / ASIC_MIDDLE_NUM;
            result = fpga_scanhash_comb(chip_config->addr, chip_config, max_nonce, mt, &mid_cb[chip_index], config->algo);
    	    break;

    	// middle 7: xdag
    	// none
    	
    	// middle 8: lyra2z
    	case ALGO_LYRA2Z:
    	    result = fpga_scanhash_lyra2z(chip_config->addr, chip_config, max_nonce, mt);
    	    break;

    	default:
    	    result = -1;
    	    break;
        }
    }
    else
    {
        // fpga mode
    	/* scan nonces for a proof-of-work hash */
    	switch (config->algo) 
    	{
        case ALGO_KECCAK:
        case ALGO_KECCAKC:
        	result = scanhash_keccak(chip_config->addr, chip_config, max_nonce, mt);
        	break;
        case ALGO_BLAKE2B:
        	result = fpga_scanhash_blake2b(chip_config->addr, chip_config, max_nonce, mt);
        	break;
    	case ALGO_DECRED:
    		result = scanhash_decred(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_BLAKE2S:
    		result = scanhash_blake2s(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_NIST5:
    		result = scanhash_nist5(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_PHI1612:
    		result = fpga_scanhash_phi1612(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_TRIBUS:
    		result = fpga_scanhash_tribus(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_SKUNK:
    		result = fpga_scanhash_skunk(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_LYRA2Z:
    		result = fpga_scanhash_lyra2z(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_PHI2:
    		result = fpga_scanhash_phi2(chip_config->addr, chip_config, max_nonce, mt);
    		break;
    	case ALGO_SIA:      // sia
    	    result = fpga_scanhash_sia(chip_config->addr, chip_config, max_nonce, mt);
    	    break;
        case ALGO_LYRA2REV3:
            result = fpga_scanhash_lyra2rev3(chip_config->addr, chip_config, max_nonce, mt);
            break;
        case ALGO_VBLAKE:
            result = fpga_scanhash_vblake(chip_config->addr, chip_config, max_nonce, mt);
            break;
        case ALGO_TRIGG:
            result = fpga_scanhash_trigg(chip_config->addr, chip_config, max_nonce, mt);
            break;
        default:
    		/* should never happen */
    		result = -1;
    	}
	}

	if (0 != result)
	{
		applog(LOG_ERR, "Generate miner work error: code = %d", result);
        sleep(1);
	}

	return result;
}

static int fulltest_nonce(const struct chip_config* chip_config, const struct miner_chip * miner_chip, struct  miner_task*  mt, uint32_t nonce)
{
	int result;
	uint8_t chip_index;

    if (opt_asic_mode)
    {
        switch (miner_chip->algo_config.algo)
    	{
    	// middle 1 
        case ALGO_LBRY:
    		result = fulltest_hash_lbry(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_SKEIN:
    		result = fulltest_hash_skein(chip_config->addr, nonce, mt);
    		break;

    	// middle 2
        case ALGO_MYR_GR:
    		result = fulltest_hash_myr(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_GROESTL:
    		result = fulltest_hash_groestl(chip_config->addr, nonce, mt);
    		break;
    		
    	// middle 3
        case ALGO_KECCAK:
    	case ALGO_KECCAKC:
    		result = fulltest_hash_keccak(chip_config->addr, nonce, mt);
    		break;
    		
    	// middle 4
        case ALGO_DECRED:
            result = fulltest_hash_decred(chip_config->addr, nonce, mt);
    		break;
        case ALGO_BLAKE2S:
    		result = fulltest_hash_blake2s(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_SIA:
    		result = fulltest_hash_sia(chip_config->addr, nonce, mt);
    		break;
        case ALGO_BLAKE2B:
    		result = fulltest_hash_blake2b(chip_config->addr, nonce, mt);
    		break;
    		
    	// middle 5: lyra2rev2
    	case ALGO_LYRA2REV2:
    	    result = fulltest_hash_lyra2rev2(chip_config->addr, nonce, mt);
            break;

    	// middle 6 comb
    	case ALGO_NIST5:
    	case ALGO_TRIBUS: 
    	case ALGO_X13: 
    	case ALGO_BCD: 
    	case ALGO_X17:
    	case ALGO_SIB:
    	case ALGO_QUBIT:
    	case ALGO_QUARK:
    	case ALGO_X11:
    	case ALGO_PHI2:
    	case ALGO_X16R:
    	case ALGO_X16S:
    	case ALGO_X16M:
    	    chip_index = (chip_config->addr - 1) / ASIC_MIDDLE_NUM;
    	    result = fulltest_hash_comb(chip_config->addr, nonce, mt, &mid_cb[chip_index], miner_chip->algo_config.algo);
    	    break;

    	// middle 7
    	// xdag

        // middle 8: lyra2z
    	case ALGO_LYRA2Z:
    	    result = fulltest_hash_lyra2z(chip_config->addr, nonce, mt);
    	    break;

    	
        default:
            break;
        }
    }
    else
    {
    	switch (miner_chip->algo_config.algo)
    	{
    	case ALGO_DECRED:
    		result = fulltest_hash_decred(chip_config->addr, nonce, mt);
    		break;        
        case ALGO_KECCAK:
    	case ALGO_KECCAKC:
    		result = fulltest_hash_keccak(chip_config->addr, nonce, mt);
    		break;     
        case ALGO_BLAKE2B:
    		result = fulltest_hash_blake2b(chip_config->addr, nonce, mt);
    		break;        
    	case ALGO_BLAKE2S:
    		result = fulltest_hash_blake2s(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_NIST5:
    		result = fulltest_hash_nist5(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_PHI1612:
    		result = fulltest_hash_phi1612(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_TRIBUS:
    		result = fulltest_hash_tribus(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_SKUNK:
    		result = fulltest_hash_skunk(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_LYRA2REV3:
    		result = fulltest_hash_lyra2rev3(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_LYRA2Z:
    		result = fulltest_hash_lyra2z(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_PHI2:
    		result = fulltest_hash_phi2(chip_config->addr, nonce, mt);
    		break;
    	case ALGO_SIA:
    		result = fulltest_hash_sia(chip_config->addr, nonce, mt);
    		break;
        case ALGO_VBLAKE:
            result = fulltest_hash_vblake(chip_config->addr, nonce, mt);
            break;
        case ALGO_TRIGG:
            result = fulltest_hash_trigg(chip_config->addr, nonce, mt);
            break;

    	default:
    		/* should never happen */
    		result = ID_ERROR;
    	}
	}
	
	/* if nonce found, submit work */
    mt->work.res_id = miner_chip->res_id;
	if (ID_NONCE_BINGO == result && !submit_work((struct thr_info*)miner_chip, &mt->work))
	{
		result = ID_SUBMIT_ERROR;
	}
	else if (ID_NONCE_HASH_ERR == result)
	{
		hash_error_count++;

        if (hash_error_max > 0) {
            time_t diff;
            if (hash_error_count % hash_error_max == 0) {
                diff = time(NULL) - hash_err_time;
                
                time(&hash_err_time);
                if (diff < hash_error_period) {
                    applog(LOG_ERR, "too many hash error(%d) every %ds, total error %d !", hash_error_max, hash_error_period, hash_error_count);
                    proper_exit(ERR_HASH_ERROR);
                }
            }
        }
	}
	return result;
}

int is_job_valid(int id)
{
	if (time(NULL) >= g_work_time_v[id] + opt_job_timeout)
	{
		return false;
	}
	else
	{
		return true;
	}
}

int receive_message(struct miner_chip* miner, struct miner_msg* message)
{
	int err = 0;
	struct msg_queue* msg_queue = &miner->msg_queue;
	
	err = pthread_mutex_lock(&miner->msg_queue.pth_mutex);
	
	if (0 != err)
	{
		applog(LOG_ERR, "chip %d lock mutex error, code = %d.", miner->chip_config.addr, err);
		proper_exit(ERR_CRITICAL);
	}

	while (0 == msg_queue->count)
	{
		pthread_cond_wait(&miner->msg_queue.pth_cond, &miner->msg_queue.pth_mutex);
	}

	memcpy(message, &msg_queue->body[msg_queue->head], sizeof(struct miner_msg));

	msg_queue->count--;
	msg_queue->head ++;

	if (MAX_MSG_NUM == msg_queue->head)
	{
		msg_queue->head = 0;
	}

	pthread_mutex_unlock(&miner->msg_queue.pth_mutex);
	if (0 != err)
	{
		applog(LOG_ERR, "chip %d lock mutex error, code = %d.", miner->chip_config.addr, err);
		proper_exit(ERR_CRITICAL);
	}

	return 0;
}

int feed_constant(struct miner_chip* miner, struct timeval* tv_fore)
{
	int err;
	int result = 0;
	struct task_queue*  task_queue = &miner->task_queue;
	const int            max_const = miner->chip_config.constant_max;
    int opt_algo = miner_cb[miner->res_id].opt_algo;

	if (is_job_valid(miner->res_id) == true)
	{
	    applog(LOG_BLUE, "%s: chip %d feed_constant", algo_names[opt_algo], miner->chip_config.addr);
	    
		while (task_queue->count != miner->chip_config.constant_max)
		{
			err = gen_miner_task(miner, &miner->chip_info, &miner->chip_config, &miner->algo_config, &task_queue->body[task_queue->tail]);

			if (0 == err)
			{
				if (task_queue->count == 0)
				{
					gettimeofday(tv_fore, NULL);
					result = 1;
				}
				task_queue->count++;
				task_queue->tail ++;

				if (task_queue->tail == max_const)
				{
					task_queue->tail = 0;
				}
			}
		}
	}
	else
	{
		result = -1;
	}

	return result;
}

int post_miner_msg(struct msg_queue* msg_queue, const struct miner_msg *miner_msg)
{
	pthread_mutex_lock(&msg_queue->pth_mutex);

	if (MAX_MSG_NUM == msg_queue->count)
	{
		pthread_mutex_unlock(&msg_queue->pth_mutex);
		return -1;
	}

	msg_queue->body[msg_queue->tail] = *miner_msg;

	msg_queue->count++;
	msg_queue->tail ++;

	if (MAX_MSG_NUM == msg_queue->tail)
	{
		msg_queue->tail = 0;
	}

	pthread_cond_signal(&msg_queue->pth_cond);
	pthread_mutex_unlock(&msg_queue->pth_mutex);

	return 0;
}

int post_miner_signal(struct miner_chip* miner, int signal)
{
	struct msg_queue*	msg_queue;
	struct miner_msg	miner_msg;

	msg_queue = &miner->msg_queue;
	gettimeofday(&miner_msg.tv_time, NULL);
	miner_msg.id = signal;
	miner_msg.data = miner->algo_config.algo;

	while (miner->state == STATE_NOT_READY) 
	    sleep(1);
		    
	if (0 != post_miner_msg(msg_queue, &miner_msg))
	{
		applog(LOG_ERR, "send to chip id %d message id %08x fail", miner->chip_config.addr, miner_msg.id);
	}
	return 0;
}

int reset_all_miners(void)
{
	struct miner_chip**	miner_chip;

	applog(LOG_WARNING, "stop all chip");

	pthread_mutex_lock(&miner_chain_mutex);

	for (miner_chip = &miner_chip_head; NULL != *miner_chip; miner_chip = &(*miner_chip)->next)
	{
        //if (miner_chip->algo_config.algo == algo)
    	post_miner_signal(*miner_chip, MSG_RESET);
	}

	pthread_mutex_unlock(&miner_chain_mutex);
}

int check_all_miners(void)
{
	struct miner_chip**	miner_chip;

	//applog(LOG_WARNING, "check all chip");

	pthread_mutex_lock(&miner_chain_mutex);

	for (miner_chip = &miner_chip_head; NULL != *miner_chip; miner_chip = &(*miner_chip)->next)
	{
		post_miner_signal(*miner_chip, MSG_CHECK);
	}

	pthread_mutex_unlock(&miner_chain_mutex);
}

int feed_all_miners(int algo)
{
	struct miner_chip**	miner_chip;

	applog(LOG_INFO, "feed all chip");

	pthread_mutex_lock(&miner_chain_mutex);

	for (miner_chip = &miner_chip_head; NULL != *miner_chip; miner_chip = &(*miner_chip)->next)
	{
        if ((*miner_chip)->algo_config.algo == algo)
    		post_miner_signal(*miner_chip, MSG_FEED);
	}

	pthread_mutex_unlock(&miner_chain_mutex);
}

void clear_all_miners(int algo)
{
	struct miner_chip**	miner_chip;
	struct msg_queue*	msg_queue;
	struct miner_msg	miner_msg;
	struct timeval		tv_time;

	gettimeofday(&tv_time, NULL);

	applog(LOG_INFO, "%s: clean all chip", algo_names[algo]);

	pthread_mutex_lock(&miner_chain_mutex);

	for (miner_chip = &miner_chip_head; NULL != *miner_chip; miner_chip = &(*miner_chip)->next)
	{
		msg_queue = &(*miner_chip)->msg_queue;
	
		miner_msg.id = MSG_CLEAN;
		miner_msg.data = algo;
		miner_msg.tv_time = tv_time;

		while ((*miner_chip)->state == STATE_NOT_READY) 
		    sleep(1);
		
		if (0 != post_miner_msg(msg_queue, &miner_msg))
		{
			applog(LOG_ERR, "send to chip id %d message id %08x fail", (*miner_chip)->chip_config.addr, miner_msg.id);
		}

	}

	pthread_mutex_unlock(&miner_chain_mutex);
}

const struct algo_version algo_ver_tab[] = 
{
    {ALGO_CR184,        0x00000000u},
    // golden = 1
	{ALGO_DECRED,       0x20000000u},
	{ALGO_KECCAK,	    0x30000000u},
	{ALGO_BLAKE2S,	    0x40000000u},
	{ALGO_BLAKE2B,	    0x50000000u},
	{ALGO_NIST5,	    0x60000000u},
	{ALGO_PHI1612,	    0x70000000u},
	{ALGO_LYRA2REV2,    0x80000000u},
	{ALGO_CR200,	    0x90000000u},
	{ALGO_SKUNK,	    0xa0000000u},
	{ALGO_TRIBUS,	    0xb0000000u},
	{ALGO_LYRA2Z,	    0xc0000000u},
	// xdag = 0xd
    // bis  = 0xe
	{ALGO_PHI2,         0xf0000000u},
	{ALGO_LYRA2REV3,    0x08000000u},
	{ALGO_VBLAKE,       0x28000000u},
	{ALGO_TRIGG,        0x38000000u}
};

int ver2algo(uint32_t version);

// argument algo not use
int check_image_version(int algo, uint32_t version)
{
#define IGNORE_VERSION  0

    int v2a = ver2algo(version);

	int tab_num = sizeof(algo_ver_tab) / sizeof(struct algo_version);
	int i = 0;

    if (opt_asic_mode)
    {
        return 0;
    }

	for (; i < tab_num; i++)
	{
		if (algo_ver_tab[i].algo == v2a)
		{
			break;
		}
	}
#if IGNORE_VERSION > 0
    return 0;
#else
	if (i == tab_num)
	{
		applog(LOG_WARNING, "not support algo");
		return -1;
	}

    /*
	if (algo_ver_tab[i].version == (version & 0xf0000000u))
		return 0;
	else
		return 1;
    */

    for (int i = 0; i < opt_algo_cnt; i++)
    {
        if (v2a == miner_cb[i].opt_algo)
            return 0;       // success
    }
#endif

    return -1;               // failure      
}

static void *miner_guard_thread(void *userdata)
{
	int err;
	int count = 0;
	int result = 0;
	struct miner_chip** miner_chip;
	int64_t interval = 0;
	int do_reset = false;
	time_t now_time;

	uint32_t ticks_count = 0;

	sleep(1);

	while(1)
	{
		sleep(1);	
		ticks_count++;

		pthread_mutex_lock(&stats_lock);
		clr_interval++;
		pthread_mutex_unlock(&stats_lock);

		check_all_miners();

		if (ticks_count % 7 == 0)
		{
			do_reset = false;
			time(&now_time);
			pthread_mutex_lock(&miner_chain_mutex);
			for (miner_chip = &miner_chip_head; NULL != *miner_chip; miner_chip = &(*miner_chip)->next)
			{
				err = pthread_mutex_lock(&(*miner_chip)->pth_mutex);
				if (0 != err)
				{
					applog(LOG_ERR, "chip %d lock mutex error, code = %d.", (*miner_chip)->chip_config.addr, err);
					proper_exit(ERR_CRITICAL);
				}

				interval = now_time - (*miner_chip)->active_time;

				// 如果 1h 没有一个yes，则清零相关状态
				if (clr_interval > 60 * 60)
				{
    				pthread_mutex_lock(&stats_lock);
    				clr_interval = 0;
    				
                	exp_rate = 0.;
                	accepted_count = 0;
                	//rejected_count = 0;
                	pthread_mutex_unlock(&stats_lock);
            	}

                if (!opt_asic_mode)
                {
    				/* 如果miner 超过30s没有响应，则复位 */
    				if (30 < interval)
    				{
    					applog(LOG_INFO, "guard check miner chip %02d, response interval = %lld", (*miner_chip)->chip_config.addr, interval);
    					do_reset = true;
    				}
				}
				pthread_mutex_unlock(&(*miner_chip)->pth_mutex);
			}
			pthread_mutex_unlock(&miner_chain_mutex);


			if (do_reset == true)
			{
				reset_all_miners();
				applog(LOG_ERR, "guard exit miner, chip response timeout: %dS", interval);
				proper_exit(ERR_CHIP_TIMEOUT);
			}
			else
			{
			    if (!opt_asic_mode)
			    {
				    broadcast_chip_msg(fd_uart);
				    //broadcast_chip_temper(fd_uart);
                    int num = opt_chip_num ? opt_chip_num : chip_num;
				    for (int k=1; k<=num; k++) {
                        fpga_write_reg(fd_uart, k, 0x31, 0x01);
				    }
				}
			}
		}

        for (int i = 0; i < opt_url_cnt; i++)
        {
            if((g_work_time_v[i] != 0) && (time(NULL) >= g_work_time_v[i] + opt_job_timeout))
            {
                applog(LOG_ERR, "wait job timeout: %dS", opt_job_timeout);
                //proper_exit(ERR_JOB_TIMEOUT);
                proper_exit(ERR_POOL_DISCONNECTED);
            }
        }
	}
	return NULL;
}

int set_active_time(struct miner_chip* miner)
{
	int err;
	err = pthread_mutex_lock(&miner->pth_mutex);
	if (0 != err)
	{
		applog(LOG_ERR, "chip %d lock mutex error, code = %d.", miner->chip_config.addr, err);
		proper_exit(ERR_CRITICAL);
	}
	time(&miner->active_time);
	pthread_mutex_unlock(&miner->pth_mutex);
}

int expect_msg_check(struct miner_chip*	miner, const struct miner_msg* miner_msg)
{
	struct timeval	tv_tmp;
	struct timeval	tv_msg1 = miner_msg->tv_time;
	struct timeval	tv_msg2 = miner->expect_msg.tv_expire;
	struct expect_msg* expect_msg  = &miner->expect_msg;

	if (expect_msg->is_expect == true)
	{
		if (expect_msg->msg_id == miner_msg->id)
		{
			applog(LOG_DEBUG, "chip %d check 0x%02x expire time %d.%06d", miner->chip_config.addr, expect_msg->msg_id, miner_msg->tv_time.tv_sec, miner_msg->tv_time.tv_usec);
			if (1 != timeval_subtract(&tv_tmp, &tv_msg1, &tv_msg2))
			{
				applog(LOG_ERR, "chip %d receive 0x%02x and timeout", miner->chip_config.addr, miner_msg->id);

				expect_msg->is_expect = false;
				return -1;
				//proper_exit(ERR_CHIP_TIMEOUT);
			}
			else
			{
				expect_msg->is_expect = false; 
			}
		}
		else
		{
			struct timeval	tv_now;
			gettimeofday(&tv_now, NULL);
			tv_msg2.tv_sec += 1;

			if (1 != timeval_subtract(&tv_tmp, &tv_now, &tv_msg2))
			{
				applog(LOG_ERR, "chip %d not receive 0x%02x and timeout", miner->chip_config.addr, expect_msg->msg_id);
				//proper_exit(ERR_CHIP_TIMEOUT);

				expect_msg->is_expect = false;
				return -1;
			}
		}
	}
	
	return 0;
}

int set_nodelay_expect_msg(struct miner_chip* miner, uint32_t msg_id)
{
	struct timeval	tv_tmp, tv_now;
	gettimeofday(&tv_now, NULL);
	tv_tmp.tv_sec  = 0;
	tv_tmp.tv_usec = opt_frame_timeout * 1000;
	timeval_add(&miner->expect_msg.tv_expire, &tv_tmp, &tv_now);
	miner->expect_msg.msg_id = msg_id;
	miner->expect_msg.is_expect = true;

	//applog(LOG_DEBUG, "chip %d set 0x%02x expire time %d.%d", miner->chip_config.addr, miner->expect_msg.msg_id, miner->expect_msg.tv_expire.tv_sec, miner->expect_msg.tv_expire.tv_usec);

	return 0;
}

char *model_name[MODEL_MAX_ID+1] = {
    // 0-7
    "K7-XC7K325T-676",
    "K7-XC6VLX365T-G1156",
    "K7-XC7K325T-G900-1",
    "V5-1157",
    "KU040",
    "K7-XC7K325T-G900-2",
    "K7-XC7K355T-1",
    "K7-XC7K420T-1",

    //8-15
    "S10",
    "A5-5AGXFB7K4",
    "A2-EP2AGX260", 
    "A52",
    "K7-XC7K420T-2",
    "K7-XC7K355T-2",
    "A5-30A",
    ""
};

// low x bits
static double calc_fract(uint32_t adc, uint32_t low_bits)
{ 
    unsigned int tmp = adc & (0xffffffffU >> (32-low_bits));
    double low = 0.;
    
    for (int j = 1; j <= low_bits; j++)
    {
        if ( (tmp & (0x1 << (low_bits-1))) != 0 )
    	    low += pow(2, -1*j);

    	tmp <<= 1;
    }
    
    return low;
}

double calc_temper(int chip_model, uint32_t adc)
{
    double temper = 0;
    
    if (chip_model == MODEL_A5 || chip_model == MODEL_A5_30A || chip_model == MODEL_A52)
    {
        int tmp = adc & 0x000000ffU;
        if (tmp < 58)
            tmp = 58;

        temper = (double)(tmp - 128);
    }
    else if (chip_model == MODEL_A2)
    {
        temper = 0;
    }
    //else if (chip_model == MODEL_A10)
    //{
    //    temper = (double)adc * (double)693 / 1024 - 265;
    //}
    else if (chip_model == MODEL_S10)
    {
        temper = adc >> 8;
        temper += calc_fract(adc, 8);
    }
    else
    {
	    temper = (double)adc * (double)503.975 / 65536 - 273.15;
	}
	
	return temper;
}

double calc_voltage(int chip_model, uint32_t adc)
{
    double voltage;

    if ((chip_model == MODEL_A5) || (chip_model == MODEL_A2) || chip_model == MODEL_A5_30A || chip_model == MODEL_A52)
    {
        voltage = 0;    // not support
    }
    //else if (chip_model == MODEL_A10)
    //{
    //    voltage = (double)adc * (double)0.019531; 
    //}
    else if (chip_model == MODEL_S10)
    {
        voltage = adc >> 16;
        voltage += calc_fract(adc, 16);
    }
    else
    {
	    voltage = (double)adc / (double)0x5555;
	}
	
	return voltage;
}

uint16_t calc_temper_adc(float temper)
{
	uint16_t adc = (temper + 273.15) * 65536 / 503.975;
	return adc;
}

int ver2algo(uint32_t version);

static void *miner_thread(void *userdata)
{
#define __PRINT_1ST_HASH    0

	struct miner_chip*	miner  = (struct miner_chip*) userdata;
	struct thr_info	*	mythr  = (struct thr_info *) miner;
    int opt_algo = miner_cb[miner->res_id].opt_algo;
	struct miner_task*  miner_task = NULL; 
	//int			thr_id = miner->res_id;
	int			algo   = miner->algo_config.algo;
	struct expect_msg* expect_msg  = &miner->expect_msg;
	time_t		   tm_rate_log = 0;
	time_t		firstwork_time = 0;
	int			err;
	char			 s[16];
	struct miner_msg     miner_msg;
	struct timeval		tv_diff;
	struct timeval		tv_tmp, tv_now;
	double compute_time = 0;
	uint32_t nonce_id;
	int result;

	bool need_exit = false;
	
	const int            max_const = miner->chip_config.constant_max;
	struct msg_queue*   msg_queue  = &miner->msg_queue;
	struct task_queue*  task_queue = &miner->task_queue;
	task_queue->body = (struct miner_task*) calloc(max_const, sizeof(struct miner_task));


	if (!task_queue->body) {
		applog(LOG_ERR, "miner->body buffer allocation failed");
		pthread_mutex_lock(&applog_lock);
		proper_exit(ERR_CRITICAL);
	}

	memset(task_queue->body, 0, max_const * sizeof(struct miner_task));

	msg_queue->body = (struct miner_msg*) calloc(MAX_MSG_NUM, sizeof(struct miner_msg));
	pthread_mutex_init(&miner->msg_queue.pth_mutex, NULL);
	pthread_cond_init (&miner->msg_queue.pth_cond, NULL);

	for (int i = 0; i < max_const; i++)
	{
		task_queue->body[i].regen_work	= true;
		task_queue->body[i].index	= i;

		if (algo == ALGO_SCRYPT) {
			task_queue->body[i].scratchbuf = scrypt_buffer_alloc(opt_scrypt_n);
			if (!task_queue->body[i].scratchbuf) {
				applog(LOG_ERR, "scrypt buffer allocation failed");
				pthread_mutex_lock(&applog_lock);
				proper_exit(ERR_CRITICAL);
			}
		}
	
		else if (algo == ALGO_PLUCK) {
			task_queue->body[i].scratchbuf = malloc(opt_pluck_n * 1024);
			if (!task_queue->body[i].scratchbuf) {
				applog(LOG_ERR, "pluck buffer allocation failed");
				pthread_mutex_lock(&applog_lock);
				proper_exit(ERR_CRITICAL);
			}
		}

		if (opt_algo == ALGO_DECRED) {
			task_queue->body[i].regen_work = true;
		} else if (opt_algo == ALGO_LBRY) {
			//regen_work = true;
		}
	}

	miner->algo_config.end_nonce = 0xffffffffU;
	miner->algo_config.wkcmp_offset = 0;
	miner->algo_config.nonce_oft = 19*sizeof(uint32_t); // 76
	miner->algo_config.wkcmp_sz = miner->algo_config.nonce_oft;

	if (jsonrpc_2) {
		miner->algo_config.wkcmp_sz  = 39;
		miner->algo_config.nonce_oft = 39;
	}

	if (algo == ALGO_DROP || algo == ALGO_ZR5) {
		// Duplicates: ignore pok in data[0]
		miner->algo_config.wkcmp_sz -= sizeof(uint32_t);
		miner->algo_config.wkcmp_offset = 1;
        } else if (opt_algo == ALGO_DECRED) {
		miner->algo_config.wkcmp_sz = 140; // 35 * 4
		miner->algo_config.nonce_oft = 140;
        } else if (opt_algo == ALGO_LBRY) {
	        miner->algo_config.wkcmp_sz  = 108; // 27
		miner->algo_config.nonce_oft = 108;
        } else if (opt_algo == ALGO_SIA) {
		miner->algo_config.nonce_oft = 32;
	        miner->algo_config.wkcmp_offset = 32 + 16;
		miner->algo_config.wkcmp_sz = 32; // 35 * 4
        }

	if (opt_chip_num == 0)
	{
		sleep(1.5);
		miner->state = STATE_INIT;
		post_miner_signal(miner, MSG_INIT);
	}
	else
	{
		miner->state = STATE_VERSION_WAIT;
		post_miner_signal(miner, MSG_VERSION_WAIT);
	}

	while(1)
	{
		receive_message(miner, &miner_msg);

        // 当 expect_msg_check 返回失败后，该线程将不再应答任何消息(串口线程和其他miner线程依旧会打印信息)
        // 如果有任何一个miner线程超过30s不响应任何消息，则guard线程会强制程序退出。
		if ( (expect_msg_check(miner, &miner_msg) < 0) || (need_exit) )
		{
		    // 读取一次constant数量
		    if (!need_exit) 
		    {
		        applog(LOG_WARNING, "miner deferred exit...");
		        applog(LOG_BLUE, "read chip %d constant number", miner->chip_config.addr);
		        fpga_read_reg(miner->chip_config.fd_uart, miner->chip_config.addr, 0x0c);
		    }
		    
		    need_exit = true;
            continue;
		}

		switch (miner->state)
		{
		case STATE_NORMAL:
			//process message
			miner_task = &task_queue->body[task_queue->head];
			switch (miner_msg.id)
			{
			case 0x0cu: 
				applog(LOG_DEBUG, "chip %d, redundant frame addr 0x%02x", miner->chip_config.addr, miner_msg.id);
				/* 收到错误的数据可能出现串口数据丢失问题，再次同步 */
				applog(LOG_DEBUG, "chip %d: do clean for missing state", miner->chip_config.addr);
				miner->state = STATE_CLEAN_WAIT;
				fpga_clear(miner->chip_config.fd_uart, miner->chip_config.addr);

				//applog(LOG_DEBUG, "chip %d: %04d", miner->chip_config.addr, __LINE__);
				set_nodelay_expect_msg(miner, 0x0Cu);
				break;
			case 0x24u: 
				result = check_image_version(miner->algo_config.algo, miner_msg.data);
				if (result != 0)
				{
					applog(LOG_ERR, "chip %d, error version = 0x%08x", miner->chip_config.addr, miner_msg.data);
					miner->chip_info.hashrate = 0;
					proper_exit(ERR_IMG_VER);
				}

				set_active_time(miner);
				break;
			case 0x64u: 
				set_active_time(miner);
				applog(LOG_INFO, "0x64: %08x", miner_msg.data);
				err = pthread_mutex_lock(&miner->pth_mutex);
				if (0 != err)
				{
					applog(LOG_ERR, "chip %d lock mutex error, code = %d.", miner->chip_config.addr, err);
					proper_exit(ERR_CRITICAL);
				}
				time(&miner->active_time);
				pthread_mutex_unlock(&miner->pth_mutex);
				miner_task->tv_start = miner_msg.tv_time;

				if (miner->chip_info.hashrate == 0)
				{
					expect_msg->is_expect = false;
				}
				else
				{
					compute_time = (miner_task->end_nonce - miner_task->start_nonce)/miner->chip_info.hashrate;
					tv_tmp.tv_sec = compute_time;
					tv_tmp.tv_usec = (compute_time - tv_tmp.tv_sec) * 1000000 + opt_frame_timeout * 1000;
					timeval_add(&expect_msg->tv_expire, &miner_task->tv_start, &tv_tmp);
					expect_msg->msg_id = 0x68u;
					expect_msg->is_expect = true;
					//applog(LOG_DEBUG, "chip %d: %04d", miner->chip_config.addr, __LINE__);
					applog(LOG_DEBUG, "chip %d set 0x%02x expire time %d.%d", miner->chip_config.addr, miner->expect_msg.msg_id, 
					                                        miner->expect_msg.tv_expire.tv_sec, miner->expect_msg.tv_expire.tv_usec);
				}

                if ( (opt_asic_mode) && (miner->chip_config.addr % 8 == 6) ) {    // only cr184 need to check fisrt hash
                    switch (miner->algo_config.algo) {
                        case ALGO_X16R: 
        				case ALGO_X16M: 
        				case ALGO_X16S: 
        				case ALGO_QUARK: 
        				case ALGO_PHI2: 
        				    // NO FIRST HASH
        				break;

                        default:
                            if ( miner_task->first_hash != miner_msg.data ) {
            				    applog(LOG_ERR, "chip %d, first hash error (0x%08x)!", miner->chip_config.addr, miner_task->first_hash);
            				    proper_exit(ERR_FIRST_HASH);
            				}
                        break;
                    }
				}
				
				break;
			case 0x68u: 
				set_active_time(miner);
				if (miner_msg.data < 1000)
				{
					//硬件返回值不精确，可能会绕回0之后
					miner_msg.data = 0xffffffffu;
				}
				miner_task->hashes_done = miner_msg.data;
				miner_task->tv_end   = miner_msg.tv_time;
				timeval_subtract(&tv_diff, &miner_task->tv_end, &miner_task->tv_start);
	
				if (tv_diff.tv_usec || tv_diff.tv_sec) {
					pthread_mutex_lock(&stats_lock);
					miner->chip_info.hashrate = miner_task->hashes_done / (tv_diff.tv_sec + tv_diff.tv_usec * 1e-6);
					pthread_mutex_unlock(&stats_lock);
				}
	
				sprintf(s, miner->chip_info.hashrate >= 1e9 ? "%.0f" : "%.2f", miner->chip_info.hashrate / 1e6);
				applog(LOG_INFO, "chip %d: %s MH/s, nonce_done: 0x%08x, constant compute elapse: %0.3fs", \
						miner->chip_config.addr, s, miner_task->hashes_done, tv_diff.tv_sec + tv_diff.tv_usec * 1e-6);
	
				//applog(LOG_INFO, "chip %d: remove work %d : %d", miner->chip_config.addr, miner->chip_config.addr, task_queue->head);
				if (task_queue->count == 0)
				{
					applog(LOG_ERR, "chip %d: chip constant number error, count can not be ZERO", miner->chip_config.addr, task_queue->count);
				}
	
				task_queue->count--;
				task_queue->head ++;
	
				if (task_queue->head == max_const)
				{
					task_queue->head = 0;
				}

				if (task_queue->count != 0)
				{
					tv_tmp.tv_sec  = 0;
					tv_tmp.tv_usec = opt_frame_timeout * 1000;
					timeval_add(&expect_msg->tv_expire, &miner_msg.tv_time, &tv_tmp);
					expect_msg->msg_id = 0x64u;
					expect_msg->is_expect = true;
					//applog(LOG_DEBUG, "chip %d: %04d", miner->chip_config.addr, __LINE__);
					applog(LOG_DEBUG, "chip %d set 0x%02x expire time %d.%d", miner->chip_config.addr, miner->expect_msg.msg_id, miner->expect_msg.tv_expire.tv_sec, miner->expect_msg.tv_expire.tv_usec);
					feed_constant(miner, &tv_now);
				}
				else
				{
					err = feed_constant(miner, &tv_now);
	
					if (err == 1)
					{
						tv_tmp.tv_sec  = 0;
						tv_tmp.tv_usec = opt_frame_timeout * 1000;
						timeval_add(&expect_msg->tv_expire, &tv_now, &tv_tmp);
						expect_msg->msg_id = 0x64u;
						expect_msg->is_expect = true;
						//applog(LOG_DEBUG, "chip %d: %04d", miner->chip_config.addr, __LINE__);
						applog(LOG_DEBUG, "chip %d set 0x%02x expire time %d.%d", miner->chip_config.addr, miner->expect_msg.msg_id, miner->expect_msg.tv_expire.tv_sec, miner->expect_msg.tv_expire.tv_usec);
	
					}

				}


				break;
			case 0x69u:
				miner->chip_info.temper = calc_temper(chip_model, miner_msg.data);
				if (miner->chip_info.temper)
				    applog(LOG_WARNING, "chip %d: temperature %0.1f", miner->chip_config.addr, miner->chip_info.temper);
				set_active_time(miner);
				break;

		    case 0x6a:
		        miner->chip_info.voltage = calc_voltage(chip_model, miner_msg.data);
		        if (miner->chip_info.voltage)
		            applog(LOG_WARNING, "chip %d: voltage %0.2fV", miner->chip_config.addr, miner->chip_info.voltage);
		        break;
		    
			case MSG_CLEAN:
                if (miner->algo_config.algo == miner_msg.data)
                {
                    applog(LOG_DEBUG, "chip %d: do clean", miner->chip_config.addr, miner->chip_config.addr);
                    miner->state = STATE_CLEAN_WAIT;
                    fpga_clear(miner->chip_config.fd_uart, miner->chip_config.addr);
                    //applog(LOG_DEBUG, "chip %d: %04d", miner->chip_config.addr, __LINE__);
                    set_nodelay_expect_msg(miner, 0x0Cu);
                }
				break;
			case MSG_RESET:
				applog(LOG_DEBUG, "chip %d: receive chip reset signal", miner->chip_config.addr);
				miner->state = STATE_RESET_WAIT;
				post_miner_signal(miner, MSG_RESET);
				break;
			case MSG_FEED:
			case MSG_INIT_FIN:
			case MSG_CLEAN_FIN:
                if (miner->algo_config.algo == miner_msg.data)
                {
                    err = feed_constant(miner, &tv_now);

                    if (err == 1)
                    {
                        tv_tmp.tv_sec  = 0;
                        tv_tmp.tv_usec = opt_frame_timeout * 1000;
                        timeval_add(&expect_msg->tv_expire, &tv_now, &tv_tmp);
                        expect_msg->msg_id = 0x64u;
                        expect_msg->is_expect = true;
                        //applog(LOG_DEBUG, "chip %d: %04d", miner->chip_config.addr, __LINE__);
                        applog(LOG_DEBUG, "chip %d set 0x%02x expire time %d.%d", miner->chip_config.addr, miner->expect_msg.msg_id, miner->expect_msg.tv_expire.tv_sec, miner->expect_msg.tv_expire.tv_usec);

                    }
                }

				break;
			default:
				if (0x70 <= miner_msg.id && miner_msg.id < 0x80)
				{
                    //miner_task->work.res_id = miner->res_id;
					err = fulltest_nonce(&miner->chip_config, miner, miner_task, miner_msg.data);
					if (ID_NONCE_BINGO == err)
					{
						//nonce_id = 0x0fu & miner_msg.id;
						//applog(LOG_INFO, "chip %d: stop nonce_id 0x%02x.", miner->chip_config.addr, nonce_id);
						//fpga_nonce_stop(miner->chip_config.fd_uart, miner->chip_config.addr, nonce_id);
					}
					set_active_time(miner);
				}
			}
			break;
		case STATE_CLEAN_WAIT:
			/* message filter */
			switch (miner_msg.id)
			{
			case 0x0c:
				if (0 != miner_msg.data)
				{
					applog(LOG_ERR, "chip %d: nonce count %d, must be ZERO", miner->chip_config.addr, miner_msg.data);
					applog(LOG_DEBUG, "chip %d: retry do clean", miner->chip_config.addr);
					miner->state = STATE_CLEAN_WAIT;
					fpga_clear(miner->chip_config.fd_uart, miner->chip_config.addr);
					//applog(LOG_DEBUG, "chip %d: %04d", miner->chip_config.addr, __LINE__);
					set_nodelay_expect_msg(miner, 0x0Cu);
				}
				else
				{
					task_queue->count = 0;
					task_queue->tail  = 0;
					task_queue->head  = 0;
					miner->state = STATE_NORMAL;
					post_miner_signal(miner, MSG_CLEAN_FIN);
				}
				break;
			case 0x24u: 
				result = check_image_version(miner->algo_config.algo, miner_msg.data);
				if (result != 0)
				{
					applog(LOG_ERR, "chip %d, error version = 0x%08x", miner->chip_config.addr, miner_msg.data);
					miner->chip_info.hashrate = 0;
					proper_exit(ERR_IMG_VER);
				}
				set_active_time(miner);
				break;
			case 0x69u:
				miner->chip_info.temper = calc_temper(chip_model, miner_msg.data);
				if (opt_warn_temp <= miner->chip_info.temper)
				{
					applog(LOG_WARNING, "chip %d: temperature %0.1f", miner->chip_config.addr, miner->chip_info.temper);
				}
				else
				{
					applog(LOG_WARNING, "chip %d: temperature %0.1f", miner->chip_config.addr, miner->chip_info.temper);
				}
				set_active_time(miner);
				break;
			case MSG_RESET:
				applog(LOG_DEBUG, "chip %d: receive chip reset signal", miner->chip_config.addr);
				miner->state = STATE_RESET_WAIT;
				post_miner_signal(miner, MSG_RESET);
				break;
			case MSG_FEED:   //如果在STATE_CLEAN_WAIT状态收到MSG_FEED/MSG_CLEAN消息，那么FPGA可能处于响应超时状态，则对其FPGA进行clear操作,使其恢复正常
			case MSG_CLEAN:
                if (miner->algo_config.algo == miner_msg.data)
                {
                    applog(LOG_DEBUG, "chip %d: do clean", miner->chip_config.addr);
                    miner->state = STATE_CLEAN_WAIT;
                    fpga_clear(miner->chip_config.fd_uart, miner->chip_config.addr);
                    set_nodelay_expect_msg(miner, 0x0Cu);
                    //applog(LOG_DEBUG, "chip %d: %04d", miner->chip_config.addr, __LINE__);
                    applog(LOG_DEBUG, "chip %d set 0x%02x expire time %d.%d", miner->chip_config.addr, miner->expect_msg.msg_id, miner->expect_msg.tv_expire.tv_sec, miner->expect_msg.tv_expire.tv_usec);
                }
				break;
			default:;
			}
			break;
		case STATE_RESET_WAIT:
			switch (miner_msg.id)
			{
			case 0x69u:
				miner->chip_info.temper = calc_temper(chip_model, miner_msg.data);
				if (opt_warn_temp <= miner->chip_info.temper)
				{
					applog(LOG_WARNING, "chip %d: temperature %0.1f", miner->chip_config.addr, miner->chip_info.temper);
				}
				else
				{
					applog(LOG_WARNING, "chip %d: temperature %0.1f", miner->chip_config.addr, miner->chip_info.temper);
				}
				set_active_time(miner);
				break;
			case MSG_RESET:
				while(1) sleep(1); 
				break;
			case MSG_RESET_FIN:
				miner->state = STATE_INIT;
				post_miner_signal(miner, MSG_INIT);
				break;
			default:;
			}
			break;
		case STATE_RETIRE:
			switch (miner_msg.id)
			{
			case 0x24u: 
				set_active_time(miner);
				break;
			case 0x69u:
				miner->chip_info.temper = calc_temper(chip_model, miner_msg.data);
				if (opt_warn_temp <= miner->chip_info.temper)
				{
					applog(LOG_WARNING, "chip %d: temperature %0.1f", miner->chip_config.addr, miner->chip_info.temper);
				}
				else
				{
					applog(LOG_WARNING, "chip %d: temperature %0.1f", miner->chip_config.addr, miner->chip_info.temper);
				}
				set_active_time(miner);
				break;
			default:;
			}
			break;
		case STATE_INIT:
			switch (miner_msg.id)
			{
			case MSG_INIT:
				fpga_init(miner->chip_config.fd_uart, miner->chip_config.addr);
				result = check_image_version(miner->algo_config.algo, miner->chip_info.image_version);
				if (result != 0)
				{
					applog(LOG_WARNING, "chip %d, error version = 0x%08x, enter retire state", miner->chip_config.addr, miner->chip_info.image_version);
					miner->state = STATE_RETIRE;
					post_miner_signal(miner, MSG_RETIRE);
				}
				else
				{
				    // asic模式 算法的旧配置，弃用！！！
					//fpga_algo_init(miner->chip_config.fd_uart, miner->algo_config.algo, miner->chip_config.addr);
					
					/* init */
					task_queue->count = 0;
					task_queue->tail  = 0;
					task_queue->head  = 0;
					miner->state = STATE_NORMAL;
					post_miner_signal(miner, MSG_INIT_FIN);
				}
				set_active_time(miner);
				break;
			default:;
			}
			break;
		case STATE_VERSION_WAIT:
			switch (miner_msg.id)
			{
			case MSG_VERSION_WAIT:
				set_active_time(miner);
				break;
			case 0x24u: 
				if (opt_chip_num > 0 && miner->chip_info.image_version == 0xffffffffu)
				{
				    if ( (miner_msg.data != 0xffffffffu) && (miner->chip_config.addr == 1) )
                	{
                	    chip_model = VERSION_2_MODEL(miner_msg.data);
                	}
	
					miner->chip_info.image_version = miner_msg.data;

					if (!opt_asic_mode)
					{
                        miner->res_id = 0;
                        int algo = ver2algo(miner->chip_info.image_version);
                        miner->algo_config.algo = algo;

                        for (int i = 0; i < opt_url_cnt; i++)
                        {
                            if (algo >= 0)
                            {
                                if (algo == (int)miner_cb[i].opt_algo)
                                {
                                    miner->res_id = i;
                                    applog(LOG_WARNING, "chip %d work on '%s'", miner->chip_config.addr, algo_names[miner_cb[i].opt_algo]);
                                }
                            }
                        }
                    }

					miner->state = STATE_INIT;
					post_miner_signal(miner, MSG_INIT);
					set_active_time(miner);
				}
				break;
			default:;
			}
			break;
		default:;
		} /* switch (miner->state) */
	}/* while(1) */

	return NULL;
}


const struct algo_addr  algo_addr_tab[] = 
{
	{ALGO_SKEIN,	1},
	{ALGO_LBRY,	    1},
	{ALGO_MYR_GR,	2},
	{ALGO_GROESTL,	2},
	{ALGO_KECCAK,	3},
	{ALGO_KECCAKC,	3},
	
	{ALGO_DECRED,	4}, // 4->1
	{ALGO_BLAKE2S,	4}, // 4->1
	{ALGO_SIA,      4}, // 4->1
	{ALGO_BLAKE2B,	4}, // 4->1
	
	{ALGO_LYRA2REV2,5},
	
	// middle 6: comb
	{ALGO_TRIBUS,	6},
	{ALGO_NIST5,	6},
	{ALGO_X13,	    6},
	{ALGO_BCD,	    6},
	{ALGO_X17,	    6},
	{ALGO_SIB,	    6},
	{ALGO_QUBIT,    6},
	{ALGO_X11,	    6},
	{ALGO_QUARK,    6},
	{ALGO_PHI2,	    6},
	{ALGO_X16R,	    6},
	{ALGO_X16S,	    6},
	{ALGO_X16M,	    6},
	
	// middle 7: xdag
	// not suport!
	
	{ALGO_LYRA2Z,	8}
};

int check_chip_algo(int addr, int algo)
{
	int tab_num = sizeof(algo_addr_tab) / sizeof(struct algo_addr);
    int ret = 1;

    if (1)
    {
        int i = 0;
        for (; i < tab_num; i++)
        {
            if (algo_addr_tab[i].algo == algo)
            {
                break;
            }
        }
        
#if IGNORE_VERSION > 0
        return 0;
#endif

        if (i == tab_num)
        {
            applog(LOG_ERR, "not support algo '%s'", algo_names[algo]);
            proper_exit(ERR_ALGO);
        }

        int m_chip = addr % CHIP_CORE_NUM;
        if (m_chip == 0) m_chip = CHIP_CORE_NUM;
        int m_algo = algo_addr_tab[i].addr % CHIP_CORE_NUM;
        if (m_algo == 0) m_algo = CHIP_CORE_NUM;
        
        if (m_chip == m_algo)
        {
            ret = 0;
        }
        else
        {
            ret = 1;
        }
    }

    return ret;
}

void* fpga_upstream_thread(void* arg)
{
	int err = 0;
	int fd_uart = *(int*)arg;
	uint8_t* p;
	struct protocal_packet msg;
	struct miner_chip*	miner_chip;
	struct miner_chip*	miner_tmp;
	struct msg_queue*	msg_queue;
	struct miner_msg	miner_msg;
	struct timeval		tv_time;

	while(1)
	{
		//applog(LOG_INFO, "Waiting for FPGA UP frame...");

		p = (uint8_t*)&msg;
		for (int i = 0; i < 6; i++)	
		{
			while(uart_recieve(fd_uart, &p[i])<=0);
		}

		applog(LOG_INFO, "FPGA UP frame: ID: %02d, Addr: 0x%02x, data: 0x%08x", msg.id, msg.addr, msg.data);
		
		gettimeofday(&tv_time, NULL);


		miner_chip = NULL;

		for (miner_tmp = miner_chip_head; NULL != miner_tmp; miner_tmp = miner_tmp->next)
		{
			if (miner_tmp->chip_config.addr == msg.id)
			{
				miner_chip = miner_tmp;
				break;
			}
		}


		if (NULL == miner_chip)     // 线程还没有创建 或者 没有找到对应ID的线程(asic模式)
		{
		    if (opt_asic_mode)
		    { 
		        continue;           // asic 模式下，不会(根据版本信息)自动创建线程，每个线程必须手动指定
		    }
		    
			if (msg.addr == 0x24)
			{
				if (opt_chip_num <= 0)
				{
					if (opt_asic_mode == false)     // fpga mode
					{
						create_miner(msg.id, msg.data, -1);
                        chip_num++;
					}
					else                            // asic mode
					{
						// asic 模式都是提前创建线程
					}
				}
				continue;
			}
			else if ( (msg.addr == 0x6a) || (msg.addr == 0x69) )
			{
			    // nothing to do 
			    continue;
			}
			else 
			{
				applog(LOG_ERR, "chip id %d error, can't find the chip (Addr: 0x%02x)", msg.id, msg.addr);
				proper_exit(ERR_UART_FRAME);
				//continue;
			}

		}

		msg_queue = &miner_chip->msg_queue;

		miner_msg.id = msg.addr;
		miner_msg.data = msg.data;
		miner_msg.tv_time = tv_time;

		while (miner_chip->state == STATE_NOT_READY) 
		    sleep(1);
		    
    	if (0 != post_miner_msg(msg_queue, &miner_msg))
    	{
    		applog(LOG_ERR, "send to chip id %d message fail", msg.id);
    	}
	}
}

static void *longpoll_thread(void *userdata)
{
	struct thr_info *mythr = (struct thr_info*) userdata;
	CURL *curl = NULL;
	char *copy_start, *hdr_path = NULL, *lp_url = NULL;
	bool need_slash = false;
    
    // thread not use
    char *rpc_url = miner_cb[0].rpc_pass;
    char *rpc_user = miner_cb[0].rpc_user;
    char *rpc_userpass = miner_cb[0].rpc_userpass;
    char *short_url = miner_cb[0].short_url;
    bool have_gbt = miner_cb[0].have_gbt;

	curl = curl_easy_init();
	if (unlikely(!curl)) {
		applog(LOG_ERR, "CURL init failed");
		goto out;
	}

start:
	hdr_path = (char*) tq_pop(mythr->q, NULL);
	if (!hdr_path)
		goto out;

	/* full URL */
	if (strstr(hdr_path, "://")) {
		lp_url = hdr_path;
		hdr_path = NULL;
	}

	/* absolute path, on current server */
	else {
		copy_start = (*hdr_path == '/') ? (hdr_path + 1) : hdr_path;
		if (rpc_url[strlen(rpc_url) - 1] != '/')
			need_slash = true;

		lp_url = (char*) malloc(strlen(rpc_url) + strlen(copy_start) + 2);
		if (!lp_url)
			goto out;

		sprintf(lp_url, "%s%s%s", rpc_url, need_slash ? "/" : "", copy_start);
	}

	if (!opt_quiet)
		applog(LOG_BLUE, "Long-polling on %s", lp_url);

	while (1) {
		json_t *val;
		char *req = NULL;
		int err;

		if (jsonrpc_2) {
			char s[128];
			pthread_mutex_lock(&rpc2_login_lock);
			if (!strlen(rpc2_id)) {
				sleep(1);
				continue;
			}
			snprintf(s, 128, "{\"method\": \"getjob\", \"params\": {\"id\": \"%s\"}, \"id\":1}\r\n", rpc2_id);
			pthread_mutex_unlock(&rpc2_login_lock);
			val = json_rpc2_call(have_gbt, curl, rpc_url, rpc_userpass, s, &err, JSON_RPC_LONGPOLL);
		} else {
			if (have_gbt) {
				req = (char*) malloc(strlen(gbt_lp_req) + strlen(lp_id) + 1);
				sprintf(req, gbt_lp_req, lp_id);
			}
			val = json_rpc_call(have_gbt, curl, rpc_url, rpc_userpass, getwork_req, &err, JSON_RPC_LONGPOLL);
			val = json_rpc_call(have_gbt, curl, lp_url, rpc_userpass,
					    req ? req : getwork_req, &err,
					    JSON_RPC_LONGPOLL);
			free(req);
		}

		if (have_stratum) {
			if (val)
				json_decref(val);
			goto out;
		}
		if (likely(val)) {
			bool rc;
			char *start_job_id;
			double start_diff = 0.0;
			json_t *res, *soval;
			res = json_object_get(val, "result");
			if (!jsonrpc_2) {
				soval = json_object_get(res, "submitold");
				submit_old = soval ? json_is_true(soval) : false;
			}
			pthread_mutex_lock(&g_work_lock_v[0]);
			start_job_id = g_work_v[0].job_id ? strdup(g_work_v[0].job_id) : NULL;
			if (have_gbt)
				rc = gbt_work_decode(res, &g_work_v[0]);
			else
				rc = work_decode(res, &g_work_v[0]);
			if (rc) {
				bool newblock = g_work_v[0].job_id && strcmp(start_job_id, g_work_v[0].job_id);
				newblock |= (start_diff != net_diff); // the best is the height but... longpoll...
				if (newblock) {
					start_diff = net_diff;
					if (!opt_quiet) {
						char netinfo[64] = { 0 };
						if (net_diff > 0.) {
							sprintf(netinfo, ", diff %.3f", net_diff);
						}
						if (opt_showdiff)
							sprintf(&netinfo[strlen(netinfo)], ", target %.3f", g_work_v[0].targetdiff);
						applog(LOG_BLUE, "%s detected new block%s", short_url, netinfo);
					}
					time(&g_work_time_v[0]);
					clear_all_miners(miner_cb[0].opt_algo);
				}
			}
			free(start_job_id);
			pthread_mutex_unlock(&g_work_lock_v[0]);
			json_decref(val);
		} else {
			pthread_mutex_lock(&g_work_lock_v[0]);
			g_work_time_v[0] -= LP_SCANTIME;
			pthread_mutex_unlock(&g_work_lock_v[0]);
			if (err == CURLE_OPERATION_TIMEDOUT) {
				clear_all_miners(miner_cb[0].opt_algo);
			} else {
				have_longpoll = false;
				clear_all_miners(miner_cb[0].opt_algo);
				free(hdr_path);
				free(lp_url);
				lp_url = NULL;
				sleep(opt_fail_pause);
				goto start;
			}
		}
	}

out:
	free(hdr_path);
	free(lp_url);
	tq_freeze(mythr->q);
	if (curl)
		curl_easy_cleanup(curl);

	return NULL;
}

static bool stratum_handle_response(int res_id, char *buf)
{
	json_t *val, *err_val, *res_val, *id_val;
	json_error_t err;
	bool ret = false;
	bool valid = false;

	val = JSON_LOADS(buf, &err);
	if (!val) {
		applog(LOG_INFO, "JSON decode failed(%d): %s", err.line, err.text);
		goto out;
	}

	res_val = json_object_get(val, "result");
	err_val = json_object_get(val, "error");
	id_val = json_object_get(val, "id");

	if (!id_val || json_is_null(id_val))
		goto out;

	if (jsonrpc_2)
	{
		if (!res_val && !err_val)
			goto out;

		json_t *status = json_object_get(res_val, "status");
		if(status) {
			const char *s = json_string_value(status);
			valid = !strcmp(s, "OK") && json_is_null(err_val);
		} else {
			valid = json_is_null(err_val);
		}
		share_result(res_id, valid, NULL, err_val ? json_string_value(err_val) : NULL);

	} else {

		if (!res_val || json_integer_value(id_val) < 4)
			goto out;
		valid = json_is_true(res_val);
		share_result(res_id, valid, NULL, err_val ? json_string_value(json_array_get(err_val, 1)) : NULL);
	}

	ret = true;

out:
	if (val)
		json_decref(val);

	return ret;
}

static void *stratum_thread(void *userdata)
{
		struct thr_info *mythr = (struct thr_info *) userdata;
		char *s;
		static int recv_err = 0;
		char *short_url = miner_cb[mythr->res_id].short_url;

		int opt_algo = miner_cb[mythr->res_id].opt_algo;
		char* rpc_url = miner_cb[mythr->res_id].rpc_url;
		char* rpc_user = miner_cb[mythr->res_id].rpc_user;
		char* rpc_pass = miner_cb[mythr->res_id].rpc_pass;
		struct stratum_ctx *stratum = &stratum_v[mythr->res_id];
		bool ret = false;
		int vblake_jobId_last = 0;

		stratum_v[mythr->res_id].url = (char*) tq_pop(mythr->q, NULL);
		if (!stratum_v[mythr->res_id].url)
				goto out;
		applog(LOG_INFO, "Starting Stratum on %s", stratum->url);

		while (1) {
				int failures = 0;

				if (stratum_need_reset) {
						stratum_need_reset = false;
						stratum_disconnect(stratum);
						if (strcmp(stratum->url, rpc_url)) {
								free(stratum->url);
								stratum->url = strdup(rpc_url);
								applog(LOG_BLUE, "Connection changed to %s", short_url);
						} else if (!opt_quiet) {
								applog(LOG_DEBUG, "Stratum connection reset");
						}
				}

				while(!stratum->curl) {
						pthread_mutex_lock(&g_work_lock_v[mythr->res_id]);
						g_work_time_v[mythr->res_id] = 0;
						pthread_mutex_unlock(&g_work_lock_v[mythr->res_id]);
						clear_all_miners(opt_algo);

						if(opt_algo == ALGO_VBLAKE)
						{
								applog(LOG_INFO, "vblake start connect");
								stratum_connect(stratum, stratum->url, opt_fail_pause);
								//sleep(1);
								s = stratum_recv_line(stratum);
								if(s == NULL)
								{
										applog(LOG_INFO, "vblake connect failed recv line"); 
										ret = false;
								}
								else
								{
										applog(LOG_INFO, "vblake connect recv: %s", s);
										ret = true;
								}

								if(ret)
								{
										ret = stratum_authorize(stratum, rpc_user, rpc_pass);
										if(ret)
												applog(LOG_INFO, "vblake auth success");
								}

								if(ret)
								{
										ret = stratum_subscribe(stratum);

										if(ret)
										{
												applog(LOG_INFO, "vblake sub suceess");
										}
								}
						}
						else{
								if (stratum_connect(stratum, stratum->url, opt_fail_pause)
												&& stratum_subscribe(stratum)
												&& stratum_authorize(stratum, rpc_user, rpc_pass)) {
										ret = true;
								}
								else{
										ret = false;
								}
						}

						if(ret == false)
						{
								stratum_disconnect(stratum);

								if (opt_retries >= 0 && ++failures > opt_retries) {
										applog(LOG_ERR, "connect to pool timeout, try %d times", failures);
										//proper_exit(ERR_POOL_TIMEOUT);
										if (stratum_connected_ever)
										{
												applog(LOG_ERR, "exit for pool disconnected!");
												proper_exit(ERR_POOL_DISCONNECTED);
										}
										else
										{
												applog(LOG_ERR, "exit for pool unreachable!");
												proper_exit(ERR_POOL_UNREACHABLE);
										}

										//applog(LOG_ERR, "...terminating workio thread");
										//tq_push(thr_info[work_thr_id].q, NULL);
										//goto out;
								}
								if (!opt_benchmark)
										applog(LOG_ERR, "...retry after %d seconds", opt_fail_pause);
								sleep(opt_fail_pause);
						}
						else
						{
								// 矿池能连上，但是不一定能够挖矿，只有能够挖矿才算连上
								//stratum_connected_ever = true;  // connected ever
						}

						if (jsonrpc_2) {
								work_free(&g_work_v[mythr->res_id]);
								work_copy(&g_work_v[mythr->res_id], &stratum->work);
						}
				}

				if(g_opt_algo == ALGO_VBLAKE)
				{
						if(stratum->job.jobId != 0 && stratum->job.jobId != vblake_jobId_last)
						{
								ret = true;
								applog(LOG_INFO, "new job clear all miners lastjob: %d newjob: %d", vblake_jobId_last, stratum->job.jobId);
								vblake_jobId_last = stratum->job.jobId;
								clear_all_miners(opt_algo);
						}
						else
						{
								ret = false;
						}
				}
				else
				{
						if (stratum->job.job_id  &&
										(!g_work_time_v[mythr->res_id] || strcmp(stratum->job.job_id, g_work_v[mythr->res_id].job_id)) )
						{
								ret = true;
						}
						else
						{
								ret = false;
						}

				}

				if(ret)
				{
						pthread_mutex_lock(&g_work_lock_v[mythr->res_id]);

						g_work_v[mythr->res_id].res_id = mythr->res_id;
						stratum_gen_work(stratum, &g_work_v[mythr->res_id]);
						time(&g_work_time_v[mythr->res_id]);
						pthread_mutex_unlock(&g_work_lock_v[mythr->res_id]);

						if (stratum->job.clean || jsonrpc_2) {
								static uint32_t last_bloc_height;
								if (!opt_quiet && last_bloc_height != stratum->bloc_height) {
										last_bloc_height = stratum->bloc_height;
										if (net_diff > 0.)
												applog(LOG_BLUE, "%s block %d, diff %.3f", algo_names[opt_algo],
																stratum->bloc_height, net_diff);
										else
												applog(LOG_BLUE, "%s %s block %d", short_url, algo_names[opt_algo],
																stratum->bloc_height);
								}
								time(&g_clean_time_v[mythr->res_id]); 
								clear_all_miners(opt_algo);
						} else if (opt_debug && !opt_quiet) {
								applog(LOG_BLUE, "%s asks job %lu for block %d", short_url,
												strtoul(stratum->job.job_id, NULL, 16), stratum->bloc_height);
								feed_all_miners(opt_algo);
						}
				}

				//applog(LOG_WARNING, "stratum_socket_full: start");
				if (!stratum_socket_full(stratum, opt_timeout)) {
						applog(LOG_ERR, "Stratum connection timeout");
						s = NULL;
				} else
						s = stratum_recv_line(stratum);
				//applog(LOG_WARNING, "stratum_socket_full: end");

				if (!s) {
						stratum_disconnect(stratum);
						applog(LOG_ERR, "Stratum connection interrupted");

						// 不能无限重连, miner退出条件为 连续5次
						if (++recv_err >= 5)
						{
								if (stratum_connected_ever)
								{
										applog(LOG_ERR, "exit for pool disconnected!");
										proper_exit(ERR_POOL_DISCONNECTED);
								}
								else
								{
										applog(LOG_ERR, "exit for pool unreachable!");
										proper_exit(ERR_POOL_UNREACHABLE);
								}
						}
						else
						{
								continue;
						}
				}

				// 矿池能连上，但是不一定能够挖矿，只有能够挖矿才算连上
				stratum_connected_ever = true;  // connected ever
				recv_err = 0;

				if (!stratum_handle_method(stratum, s) && g_opt_algo != ALGO_VBLAKE)
				{
						stratum_handle_response(mythr->res_id, s);
				}
				free(s);
		}
out:
		return NULL;
}

static void show_version_and_exit(void)
{
	printf(" built "
#ifdef _MSC_VER
	 "with VC++ %d", msver());
#elif defined(__GNUC__)
	 "with GCC ");
	printf("%d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
	printf(" the " __DATE__ "\n");

	// Note: if compiled with cpu opts (instruction sets),
	// the binary is no more compatible with older ones!
	printf(" compiled for"
#if defined(__ARM_NEON__)
		" ARM NEON"
#elif defined(__AVX2__)
		" AVX2"
#elif defined(__AVX__)
		" AVX"
#elif defined(__XOP__)
		" XOP"
#elif defined(__SSE4_1__)
		" SSE4"
#elif defined(_M_X64) || defined(__x86_64__)
		" x64"
#elif defined(_M_IX86) || defined(__x86__)
		" x86"
#else
		" general use"
#endif
		"\n");

	printf(" config features:"
#if defined(USE_ASM) && defined(__i386__)
		" i386"
#endif
#if defined(USE_ASM) && defined(__x86_64__)
		" x86_64"
#endif
#if defined(USE_ASM) && (defined(__i386__) || defined(__x86_64__))
		" SSE2"
#endif
#if defined(__x86_64__) && defined(USE_XOP)
		" XOP"
#endif
#if defined(__x86_64__) && defined(USE_AVX)
		" AVX"
#endif
#if defined(__x86_64__) && defined(USE_AVX2)
		" AVX2"
#endif
#if defined(USE_ASM) && defined(__arm__) && defined(__APCS_32__)
		" ARM"
#if defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5TE__) || \
	defined(__ARM_ARCH_5TEJ__) || defined(__ARM_ARCH_6__) || \
	defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || \
	defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_6T2__) || \
	defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
	defined(__ARM_ARCH_7__) || \
	defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || \
	defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
		" ARMv5E"
#endif
#if defined(__ARM_NEON__)
		" NEON"
#endif
#endif
		"\n\n");
	/* dependencies versions */
	printf("%s\n", curl_version());
#ifdef JANSSON_VERSION
	printf("jansson/%s ", JANSSON_VERSION);
#endif
#ifdef PTW32_VERSION
	printf("pthreads/%d.%d.%d.%d ", PTW32_VERSION);
#endif
	printf("\n");
	exit(0);
}

static void show_usage_and_exit(int status)
{
	if (status)
		fprintf(stderr, "Try `" PACKAGE_NAME " --help' for more information.\n");
	else
		printf(usage);
	exit(status);
}

static void strhide(char *s)
{
	if (*s) *s++ = 'x';
	while (*s) *s++ = '\0';
}

void parse_arg(int key, char *arg)
{
	char *p;
	int v, i;
	uint64_t ul;
	double d;

	switch(key) {
	case 'M':   // middle addr: 1 ~ 255
	    middle_stage[opt_algo_cnt][middle_stage_ix++] = atoi(arg);
	    opt_chip_num++;
	    break;
	    
	case 'a':
		for (i = 0; i < ALGO_COUNT; i++) {
			v = (int) strlen(algo_names[i]);
			if (v && !strncasecmp(arg, algo_names[i], v)) {
				if (arg[v] == '\0') {
					miner_cb[opt_algo_cnt++].opt_algo = (enum algos) i;
					middle_stage_ix = 0;
					break;
				}
				if (arg[v] == ':') {
					char *ep;
					v = strtol(arg+v+1, &ep, 10);
					if (*ep || (i == ALGO_SCRYPT && v & (v-1)) || v < 2)
						continue;
                    if (opt_algo_cnt < MAX_POOL_NUM) {
					    miner_cb[opt_algo_cnt++].opt_algo = (enum algos)i;
					    middle_stage_ix = 0;
                    }
					opt_scrypt_n = v;
					break;
				}
			}
		}

		int opt_algo = miner_cb[opt_algo_cnt - 1].opt_algo;
        g_opt_algo = opt_algo;

		if (i == ALGO_COUNT) {

			if (strstr(arg, ":")) {
				// pick and strip the optional factor
				char *nf = strstr(arg, ":");
				opt_scrypt_n = strtol(&nf[1], NULL, 10);
				*nf = '\0';
			}

			// some aliases...
			if (!strcasecmp("blake2", arg))
				i = opt_algo = ALGO_BLAKE2S;
			else if (!strcasecmp("cryptonight-light", arg))
				i = opt_algo = ALGO_CRYPTOLIGHT;
			else if (!strcasecmp("flax", arg))
				i = opt_algo = ALGO_C11;
			else if (!strcasecmp("diamond", arg))
				i = opt_algo = ALGO_DMD_GR;
			else if (!strcasecmp("droplp", arg))
				i = opt_algo = ALGO_DROP;
			else if (!strcasecmp("jackpot", arg))
				i = opt_algo = ALGO_JHA;
			else if (!strcasecmp("lyra2", arg))
				i = opt_algo = ALGO_LYRA2;
			else if (!strcasecmp("lyra2v2", arg))
				i = opt_algo = ALGO_LYRA2REV2;
			else if (!strcasecmp("scryptjane", arg))
				i = opt_algo = ALGO_SCRYPTJANE;
			else if (!strcasecmp("sibcoin", arg))
				i = opt_algo = ALGO_SIB;
			else if (!strcasecmp("timetravel10", arg))
				i = opt_algo = ALGO_BITCORE;
			else if (!strcasecmp("ziftr", arg))
				i = opt_algo = ALGO_ZR5;
			else
				applog(LOG_ERR, "Unknown algo parameter '%s'", arg);
		}
		if (i == ALGO_COUNT) {
			show_usage_and_exit(1);
		}
		if (!opt_nfactor && opt_algo == ALGO_SCRYPT)
			opt_nfactor = 9;
		if (opt_algo == ALGO_SCRYPTJANE && opt_scrypt_n == 0)
			opt_scrypt_n = 5;
		break;
	case 'A':
		opt_asic_mode = true;
		break;
	case 'b':
		p = strstr(arg, ":");
		if (p) {
			/* ip:port */
			if (p - arg > 0) {
				free(opt_api_allow);
				opt_api_allow = strdup(arg);
				opt_api_allow[p - arg] = '\0';
			}
			opt_api_listen = atoi(p + 1);
		}
		else if (arg && strstr(arg, ".")) {
			/* ip only */
			free(opt_api_allow);
			opt_api_allow = strdup(arg);
		}
		else if (arg) {
			/* port or 0 to disable */
			opt_api_listen = atoi(arg);
		}
		break;

	case 1101: /* --hash-error-max */
		hash_error_max = atoi(arg);
        applog(LOG_WARNING, "set hash_error_max: %d", hash_error_max);
		break;
	case 1102: /* --hash-error-period */
		hash_error_period = atoi(arg);
        applog(LOG_WARNING, "set hash_error_period: %d", hash_error_period);
		break;

	case 1030: /* --api-remote */
		opt_api_remote = 1;
		break;
	case 'n':
		if (opt_algo == ALGO_NEOSCRYPT) {
			v = atoi(arg);
			/* Nfactor = lb(N) - 1; N = (1 << (Nfactor + 1)) */
			if ((v < 0) || (v > 30)) {
				fprintf(stderr, "incorrect Nfactor %d\n", v);
				show_usage_and_exit(1);
			}
			opt_nfactor = v;
		}
		break;
	case 'B':
		opt_background = true;
		use_colors = false;
		break;
	case 'c': {
		json_error_t err;
		json_t *config;
		if (arg && strstr(arg, "://")) {
			config = json_load_url(arg, &err);
		} else {
			config = JSON_LOADF(arg, &err);
		}
		if (!json_is_object(config)) {
			if (err.line < 0)
				fprintf(stderr, "%s\n", err.text);
			else
				fprintf(stderr, "%s:%d: %s\n",
					arg, err.line, err.text);
		} else {
			parse_config(config, arg);
			json_decref(config);
		}
		break;
	}
	case 'q':
		opt_quiet = true;
		break;
	case 'D':
		opt_debug = true;
		if(g_opt_algo == ALGO_VBLAKE)
		{
				opt_debug = false;
		}
		break;
	case 'p':
        if (opt_url_cnt > 0) {
    		free(miner_cb[opt_url_cnt-1].rpc_pass);
		    miner_cb[opt_url_cnt-1].rpc_pass = strdup(arg);
        }
		strhide(arg);
		break;
	case 'P':
		opt_protocol = true;
		break;
	case 'r':
		v = atoi(arg);
		if (v < -1 || v > 9999) /* sanity check */
			show_usage_and_exit(1);
		opt_retries = v;
		break;
	case 'R':
		v = atoi(arg);
		if (v < 1 || v > 9999) /* sanity check */
			show_usage_and_exit(1);
		opt_fail_pause = v;
		break;
	case 's':
		v = atoi(arg);
		if (v < 1 || v > 9999) /* sanity check */
			show_usage_and_exit(1);
		opt_scantime = v;
		break;
	case 'T':
		v = atoi(arg);
		if (v < 1 || v > 99999) /* sanity check */
			show_usage_and_exit(1);
		opt_timeout = v;
		break;
	case 'd':
		free(device_name );
		device_name  = strdup(arg);
		break;
	case 't':
		break;
	case 'u':
        if (opt_url_cnt > 0) {
		    free(miner_cb[opt_url_cnt-1].rpc_user);
		    miner_cb[opt_url_cnt-1].rpc_user = strdup(arg);
        }
		break;
	case 'o': {			/* --url */
		char *ap, *hp;
		ap = strstr(arg, "://");
		ap = ap ? ap + 3 : arg;
		hp = strrchr(arg, '@');
		if (hp) {
			*hp = '\0';
			p = strchr(ap, ':');
			if (p) {
				free(miner_cb[opt_url_cnt].rpc_userpass);
				miner_cb[opt_url_cnt].rpc_userpass = strdup(ap);
				free(miner_cb[opt_url_cnt].rpc_user);
				miner_cb[opt_url_cnt].rpc_user = (char*) calloc(p - ap + 1, 1);
				strncpy(miner_cb[opt_url_cnt].rpc_user, ap, p - ap);
				free(miner_cb[opt_url_cnt].rpc_pass);
				miner_cb[opt_url_cnt].rpc_pass = strdup(++p);
				if (*p) *p++ = 'x';
				v = (int) strlen(hp + 1) + 1;
				memmove(p + 1, hp + 1, v);
				memset(p + v, 0, hp - p);
				hp = p;
			} else {
				free(miner_cb[opt_url_cnt].rpc_user);
				miner_cb[opt_url_cnt].rpc_user = strdup(ap);
			}
			*hp++ = '@';
		} else
			hp = ap;

		if (ap != arg) {
			if (strncasecmp(arg, "http://", 7) &&
			    strncasecmp(arg, "https://", 8) &&
			    strncasecmp(arg, "stratum+tcp://", 14)) {
				fprintf(stderr, "unknown protocol -- '%s'\n", arg);
				show_usage_and_exit(1);
			}
			free(miner_cb[opt_url_cnt].rpc_url);
            if (opt_url_cnt < MAX_POOL_NUM) {
			    miner_cb[opt_url_cnt].rpc_url = strdup(arg);
            }
			strcpy(miner_cb[opt_url_cnt].rpc_url + (ap - arg), hp);
			miner_cb[opt_url_cnt].short_url = &((miner_cb[opt_url_cnt].rpc_url)[ap - arg]);

            opt_url_cnt++;
		} else {
			if (*hp == '\0' || *hp == '/') {
				fprintf(stderr, "invalid URL -- '%s'\n",
					arg);
				show_usage_and_exit(1);
			}
			free(miner_cb[opt_url_cnt].rpc_url);
			miner_cb[opt_url_cnt].rpc_url = (char*) malloc(strlen(hp) + 8);
			sprintf(miner_cb[opt_url_cnt].rpc_url, "http://%s", hp);
			miner_cb[opt_url_cnt].short_url = &(miner_cb[opt_url_cnt].rpc_url)[sizeof("http://")-1];

            opt_url_cnt++;
		}
		have_stratum = !opt_benchmark && !strncasecmp(miner_cb[opt_url_cnt-1].rpc_url, "stratum", 7);
		break;
	}
	case 'O':			/* --userpass */
		p = strchr(arg, ':');
		if (!p) {
			fprintf(stderr, "invalid username:password pair -- '%s'\n", arg);
			show_usage_and_exit(1);
		}
		free(miner_cb[opt_url_cnt].rpc_userpass);
		miner_cb[opt_url_cnt].rpc_userpass = strdup(arg);
		free(miner_cb[opt_url_cnt].rpc_user);
		miner_cb[opt_url_cnt].rpc_user = (char*) calloc(p - arg + 1, 1);
		strncpy(miner_cb[opt_url_cnt].rpc_user, arg, p - arg);
		free(miner_cb[opt_url_cnt].rpc_pass);
		miner_cb[opt_url_cnt].rpc_pass = strdup(++p);
		strhide(p);
		break;
	case 'x':			/* --proxy */
		if (!strncasecmp(arg, "socks4://", 9))
			opt_proxy_type = CURLPROXY_SOCKS4;
		else if (!strncasecmp(arg, "socks5://", 9))
			opt_proxy_type = CURLPROXY_SOCKS5;
#if LIBCURL_VERSION_NUM >= 0x071200
		else if (!strncasecmp(arg, "socks4a://", 10))
			opt_proxy_type = CURLPROXY_SOCKS4A;
		else if (!strncasecmp(arg, "socks5h://", 10))
			opt_proxy_type = CURLPROXY_SOCKS5_HOSTNAME;
#endif
		else
			opt_proxy_type = CURLPROXY_HTTP;
		free(opt_proxy);
		opt_proxy = strdup(arg);
		break;
	case 1001:
		free(opt_cert);
		opt_cert = strdup(arg);
		break;
	case 1002:
		use_colors = false;
		break;
	case 1003:
		want_longpoll = false;
		break;
	case 1005:
		opt_benchmark = true;
		want_longpoll = false;
		want_stratum = false;
		have_stratum = false;
		break;
	case 1006:
		print_hash_tests();
		exit(0);
	case 1007:
		want_stratum = false;
		opt_extranonce = false;
		break;
	case 1008:
		opt_time_limit = atoi(arg);
		break;
	case 1009:
		opt_redirect = false;
		break;
	case 1010:
		allow_getwork = false;
		break;
	case 1011:
		//miner_cb[opt_url_cnt].have_gbt = false;
		//miner_cb[opt_url_cnt-1].have_gbt = false;
		miner_cb[opt_url_cnt].have_gbt = false;
		break;
	case 1012:
		opt_extranonce = false;
		break;
	case 1013:
		opt_showdiff = true;
		break;
	case 1014:
		opt_showdiff = false;
		break;
	case 1016:			/* --coinbase-addr */
		pk_script_size = address_to_script(pk_script, sizeof(pk_script), arg);
		if (!pk_script_size) {
			fprintf(stderr, "invalid address -- '%s'\n", arg);
			show_usage_and_exit(1);
		}
		break;
	case 1015:			/* --coinbase-sig */
		if (strlen(arg) + 1 > sizeof(coinbase_sig)) {
			fprintf(stderr, "coinbase signature too long\n");
			show_usage_and_exit(1);
		}
		strcpy(coinbase_sig, arg);
		break;
	case 'f':
		d = atof(arg);
		if (d == 0.)	/* --diff-factor */
			show_usage_and_exit(1);
		miner_cb[opt_url_cnt-1].opt_diff_factor = d;
		break;
	case 'm':
		d = atof(arg);
		if (d == 0.)	/* --diff-multiplier */
			show_usage_and_exit(1);
		miner_cb[opt_url_cnt-1].opt_diff_factor = 1.0/d;
		break;
	case 'S':
		use_syslog = true;
		use_colors = false;
		break;
	case 1019: // max-log-rate
		opt_maxlograte = atoi(arg);
		break;
	case 1020:
		p = strstr(arg, "0x");
		if (p)
			ul = strtoul(p, NULL, 16);
		else
			ul = atol(arg);
		if (ul > (1UL<<num_cpus)-1)
			ul = -1;
		opt_affinity = ul;
		break;
	case 1021:
		v = atoi(arg);
		if (v < 0 || v > 5)	/* sanity check */
			show_usage_and_exit(1);
		opt_priority = v;
		break;
	case 1060: // max-temp
		d = atof(arg);
		opt_max_temp = d;
		break;
	case 1061: // max-diff
		d = atof(arg);
		opt_max_diff = d;
		break;
	case 1062: // max-rate
		d = atof(arg);
		p = strstr(arg, "K");
		if (p) d *= 1e3;
		p = strstr(arg, "M");
		if (p) d *= 1e6;
		p = strstr(arg, "G");
		if (p) d *= 1e9;
		opt_max_rate = d;
		break;
	case 1024:
		opt_randomize = true;
		break;
	case 'V':
		show_version_and_exit();
	case 'h':
		show_usage_and_exit(0);
	case 'C':
		v = atoi(arg);
		if (v > 128) /* sanity check */
			show_usage_and_exit(1);

		opt_chip_num += v;
		break;
	case 'F':
		d = atof(arg);
		if ( (d < 0.) || (d > 1.0) )
			show_usage_and_exit(1);

		// manager 给出的参数是拒绝率上限，而miner需要的是提交率下限，即
		// d + opt_accept_limit = 1
		opt_accept_limit = 1.0 - d;
		break;
	case 'J':
		d = atoi(arg);	/* job timeout */
		if (d == 0.)
			show_usage_and_exit(1);
		opt_job_timeout = d;
		break;
	case 2000: // warn temperature 
		d = atof(arg);
		opt_warn_temp = d;
		break;
	default:
		show_usage_and_exit(1);
	}
}

void parse_config(json_t *config, char *ref)
{
	int i;
	json_t *val;

	for (i = 0; i < ARRAY_SIZE(options); i++) {
		if (!options[i].name)
			break;

		val = json_object_get(config, options[i].name);
		if (!val)
			continue;
		if (options[i].has_arg && json_is_string(val)) {
			char *s = strdup(json_string_value(val));
			if (!s)
				break;
			parse_arg(options[i].val, s);
			free(s);
		}
		else if (options[i].has_arg && json_is_integer(val)) {
			char buf[16];
			sprintf(buf, "%d", (int)json_integer_value(val));
			parse_arg(options[i].val, buf);
		}
		else if (options[i].has_arg && json_is_real(val)) {
			char buf[16];
			sprintf(buf, "%f", json_real_value(val));
			parse_arg(options[i].val, buf);
		}
		else if (!options[i].has_arg) {
			if (json_is_true(val))
				parse_arg(options[i].val, "");
		}
		else
			applog(LOG_ERR, "JSON option %s invalid",
			options[i].name);
	}
}

static void parse_cmdline(int argc, char *argv[])
{
	int key;

	while (1) {
#if HAVE_GETOPT_LONG
		key = getopt_long(argc, argv, short_options, options, NULL);
#else
		key = getopt(argc, argv, short_options);
#endif
		if (key < 0)
			break;

		parse_arg(key, optarg);
	}
	if (optind < argc) {
		fprintf(stderr, "%s: unsupported non-option argument -- '%s'\n",
			argv[0], argv[optind]);
		show_usage_and_exit(1);
	}
}

#ifndef WIN32
static void signal_handler(int sig)
{
	switch (sig) {
	case SIGHUP:
		applog(LOG_INFO, "SIGHUP received");
		break;
	case SIGINT:
		applog(LOG_INFO, "SIGINT received, exiting");
		proper_exit(0);
		break;
	case SIGTERM:
		applog(LOG_INFO, "SIGTERM received, exiting");
		proper_exit(0);
		break;
	}
}
#else
BOOL WINAPI ConsoleHandler(DWORD dwType)
{
	switch (dwType) {
	case CTRL_C_EVENT:
		applog(LOG_INFO, "CTRL_C_EVENT received, exiting");
		proper_exit(0);
		break;
	case CTRL_BREAK_EVENT:
		applog(LOG_INFO, "CTRL_BREAK_EVENT received, exiting");
		proper_exit(0);
		break;
	default:
		return false;
	}
	return true;
}
#endif

static int thread_create(struct thr_info *thr, void* func)
{
	int err = 0;
	pthread_attr_init(&thr->attr);
	err = pthread_create(&thr->pth, &thr->attr, func, thr);
	pthread_attr_destroy(&thr->attr);
	return err;
}

static void show_credits()
{
	printf("** " PACKAGE_NAME " " PACKAGE_VERSION " by cloudersemi.com **\n");
}

int get_miner_algo(int res_id)
{
	return miner_cb[res_id].opt_algo;
}

void get_defconfig_path(char *out, size_t bufsize, char *argv0);

int ver2algo(uint32_t version)
{
    int algo = -1;
    version &= 0xff000000;


    int tab_num = sizeof(algo_ver_tab) / sizeof(struct algo_version);
    
    for (int i = 0; i < tab_num; i++)
    {
        if (algo_ver_tab[i].version == version)
        {
            algo = algo_ver_tab[i].algo;
            break;
        }
    }

    return algo;
}

int create_miner(int addr, uint32_t version, int res_id)
{
	int count = 0;
	int result = 0;
	struct miner_chip** miner_chip;

	pthread_mutex_lock(&miner_chain_mutex);

	struct miner_chip*  tmp = (struct miner_chip*) calloc(1, sizeof(struct miner_chip));
	memset(tmp, 0, sizeof(struct miner_chip));
	tmp->next = NULL;

	for (miner_chip = &miner_chip_head; NULL != *miner_chip; miner_chip = &(*miner_chip)->next)
	{
		count++;
		if ((*miner_chip)->chip_config.addr == addr)
		{
			applog(LOG_ERR, "chip id addr is duplicate");
			free(tmp);
			result = -1;
			goto EXIT;
		}
	}

    // STATE_NOT_READY 状态用于：禁止其他线程在miner线程完全启动之前向其发送消息，否则导致段错误.
    tmp->state = STATE_NOT_READY;
	*miner_chip = tmp;

	//tmp->algo_config.algo		= miner_cb[0].opt_algo;
	tmp->chip_config.fd_uart	= fd_uart;
	tmp->chip_config.constant_max	= 2;
	tmp->chip_config.addr		= addr;
	tmp->chip_config.index		= count;

	tmp->chip_info.image_version    = version;
	if ( (version != 0xffffffffu) && (addr == 1) )
	{
	    chip_model = VERSION_2_MODEL(version);
	}

    tmp->algo_config.algo = ver2algo(version);
    tmp->res_id = 0;
    int algo = tmp->algo_config.algo;

    if (opt_asic_mode)
    {
        tmp->res_id = res_id;
        tmp->algo_config.algo = miner_cb[res_id].opt_algo;
    }
    else 
    {
        for (int i = 0; i < opt_url_cnt; i++)
        {
            if (algo >= 0)
            {
                if (algo == (int)miner_cb[i].opt_algo)
                {
                    tmp->res_id = i;
                    applog(LOG_WARNING, "chip %d work on '%s'", addr, algo_names[miner_cb[i].opt_algo]);
                }
            }
        }
    }

	if (opt_chip_num <= 0)
	{
		miner_chip_count = count + 1;
	}

	pthread_mutex_init(&tmp->pth_mutex, NULL);
	pthread_attr_init(&tmp->thr_info.attr);
	pthread_create(&tmp->thr_info.pth, &tmp->thr_info.attr, miner_thread, tmp);
	pthread_attr_destroy(&tmp->thr_info.attr);

EXIT:
	pthread_mutex_unlock(&miner_chain_mutex);

	return result;
}

int get_host_type(void)
{
	struct utsname info;
	if(uname(&info))
	{
		applog(LOG_WARNING, "get computer infomation fail.");
		return -1;	
	}
	applog(LOG_INFO, "host arch: %s", info.machine);
	
	if (strstr(info.machine, "armv7l"))
	{
		return HOST_RASPBERRY;
	}
	else
	{
		return HOST_X86;
	}
}

int print_arg(int argc, char *argv[])
{
	int len = 0;
	char	*s = 0;
	int offset = 0;
	
	for (int i = 0; i < argc; i++)
	{
		len += strlen(argv[i]);
	}

	if (len != 0)
	{
		char *s = (char*) malloc(len + argc + 1);
		for (int i = 0; i < argc; i++)
		{
			offset += sprintf(s + offset, "%s ", argv[i]);
		}
		s[offset-1] = '\0';
		applog(LOG_INFO, "argument: %s", s);
		free(s);
	}
	else
	{
		applog(LOG_ERR, "argument: NO ARGUMENT");
	}

	return 0;
}

char all_algo_str[MAX_POOL_NUM*16] = {0};

int format_miner_status(void* buffer)
{
	struct miner_status* status = (struct miner_status*)buffer;
	status->version = PACKAGE_VERSION; 

    int num = 0;
    memset(all_algo_str, 0, sizeof(all_algo_str));
    for (int i = 0; i < opt_url_cnt; i++)
    {
        num += sprintf(all_algo_str, "%s,", algo_names[miner_cb[i].opt_algo]);
    }
    if (num > 0)
        all_algo_str[num-1] = '\0';
    
	status->algo = all_algo_str;

	status->hashrate_cur = calc_all_hashrate();
	status->hashrate_cur /= 1000000;
	status->hashrate_avg = status->hashrate_cur;
	status->solved_count = solved_count;
	status->accepted_count = accepted_count;
	if (is_job_valid(0))
		status->diff = stratum_v[0].job.diff;
	else
		status->diff = 0;

	status->curtime = time(NULL);
	status->uptime =  status->curtime- startup_time;
	status->hash_error = hash_error_count;
	return 0;
}

int format_miner_temper(char* buffer, int len)
{
	int i = 0;
	int err;
	struct miner_chip** miner_chip;

	pthread_mutex_lock(&miner_chain_mutex);
	for (miner_chip = &miner_chip_head; NULL != *miner_chip; miner_chip = &(*miner_chip)->next)
	{
		err = pthread_mutex_lock(&(*miner_chip)->pth_mutex);
		if (0 != err)
		{
			applog(LOG_ERR, "chip %d lock mutex error, code = %d.", (*miner_chip)->chip_config.addr, err);
			proper_exit(ERR_CRITICAL);
		}

		i += sprintf(buffer + i, "%0.1f,", (*miner_chip)->chip_info.temper);

		pthread_mutex_unlock(&(*miner_chip)->pth_mutex);

		if (i + 10 > len)
		{
			break;
		}
	}
	pthread_mutex_unlock(&miner_chain_mutex);

	if (i > 1)
	{
		buffer[i-1] = '\0';
		return i - 1;
	}
	else
	{
		return i;
	}
}

void middles_config(void)
{
    for (int j = 0; j < opt_algo_cnt; j++)
    {
        int index = 0;
        while (middle_stage[j][index]) {
            int res_id = -1;
            middle_cb_t *mcb = 0;
            int algo = miner_cb[j].opt_algo;

            int addr = middle_stage[j][index];
            int mid = addr % (ASIC_MIDDLE_NUM);
            if (mid == 0) 
                mid = ASIC_MIDDLE_NUM;
                
            uint8_t chip_index = (addr-1) / ASIC_MIDDLE_NUM;
            mcb = &mid_cb[chip_index];
        
            switch (mid)
            {
            case ASIC_MIDDLE_1:
                if (algo == ALGO_LBRY)          // 0x10: [0]=1,[1]=1,[3]=1; 0x0d: [0]=1,[2]=1,[3]=0; 0x0f: [0]=1,[1:3]=7, 0x1e: [0:4]=11
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x0b);  
                    set_share_algo(mcb, addr, ((mcb->share_algo & 0xfffffff7) | 0x5));
                    set_hash_conf(mcb, addr, mcb->hash_conf | 0x0f);
                    set_nonce_pos(mcb, addr, (mcb->nonce_pos & 0xffffffe0) | (11L << 0));
                }
                else if (algo == ALGO_SKEIN)    // 0x10: [1]=1,[2]=1; 0x0d: [0]=0,[3]=0; 0x0f: [0]=1,[1:3]=7, 0x1e: [5:9]=3
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x06); 
                    set_share_algo(mcb, addr, mcb->share_algo & 0xfffffff6);
                    set_hash_conf(mcb, addr, mcb->hash_conf | 0x0f);
                    set_nonce_pos(mcb, addr, (mcb->nonce_pos & 0xfffffc1f) | (3L << 5));
                }
                break;
            case ASIC_MIDDLE_2:
                if (algo == ALGO_MYR_GR)        // 0x10: [0]=1,[4]=1; 0x0d: [1]=1,[2]=0; 0x0f: [4]=0,[5:7]=7, 0x1e: [10:14]=19
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x11);  
                    set_share_algo(mcb, addr, ((mcb->share_algo & 0xfffffffb) | 0x2));
                    set_hash_conf(mcb, addr, mcb->hash_conf | 0xe0);
                    set_nonce_pos(mcb, addr, (mcb->nonce_pos & 0xffff83ff) | (19L << 10));
                }
                else if (algo == ALGO_GROESTL)  // 0x10: [4]=1; 0x0d: [1]=0; 0x0f: [4]=0,[5:7]=7, 0x1e: [10:14]=19
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x10); 
                    set_share_algo(mcb, addr, mcb->share_algo & 0xfffffffd);
                    set_hash_conf(mcb, addr, (mcb->hash_conf & 0xffffffef) | 0xe0);
                    set_nonce_pos(mcb, addr, (mcb->nonce_pos & 0xffff83ff) | (19L << 10));
                }
                break;
            case ASIC_MIDDLE_3:
                if (algo == ALGO_KECCAK)        // 0x10: [5]=1; 0x0f: [8]=0,[11:9]=7, 0x1e: [15:19]=19
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x20);  
                    set_hash_conf(mcb, addr, (mcb->hash_conf & 0xfffffeff) | 0xe00L);
                    set_nonce_pos(mcb, addr, (mcb->nonce_pos & 0xfff07fff) | (19L << 15));
                }
                break;
            case ASIC_MIDDLE_4:
                if (algo == ALGO_DECRED)        // blake256, 0x10: [6]=1; 0x0e: [7:4]=0xe,[3:0]=1; 0x0f: [12]=1, [15:13]=7, 0x1f: [0:4]=3
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x40);  
                    set_blake_conf(mcb, addr, (mcb->blake_conf & 0xffffff00) | 0xe1);
                    set_hash_conf(mcb, addr, mcb->hash_conf | 0xf000L);
                    set_nonce_pos_ex(mcb, addr, (mcb->nonce_pos_ex & 0xfffffffe0) | (3L << 0));
                }
                else if (algo == ALGO_BLAKE2S)  // blake2s, 0x10: [6]=1; 0x0e: [7:4]=0xa,[3:0]=2; 0x0f: [12]=0, [15:13]=7, 0x1f: [0:4]=3
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x40); 
                    set_blake_conf(mcb, addr, (mcb->blake_conf & 0xffffff00) | 0xa2);
                    set_hash_conf(mcb, addr, (mcb->hash_conf & 0xffffefff) | 0xe000L);
                    set_nonce_pos_ex(mcb, addr, (mcb->nonce_pos_ex & 0xfffffffe0) | (3L << 0));
                }
                else if (algo == ALGO_SIA)      // blake2b sia, 0x10: [6]=1; 0x0e: [7:4]=0xc,[3:0]=4; 0x0f: [12]=1, [15:13]=0, 0x1f: [0:4]=8
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x40); 
                    set_blake_conf(mcb, addr, (mcb->blake_conf & 0xffffff00) | 0xc4);
                    set_hash_conf(mcb, addr, (mcb->hash_conf & 0xffff1fff) | 0x1000);
                    set_nonce_pos_ex(mcb, addr, (mcb->nonce_pos_ex & 0xfffffffe0) | (8L << 0));
                }
                else if (algo == ALGO_BLAKE2B)  // blake2b bcx, 0x10: [6]=1; 0x0e: [7:4]=0xc,[3:0]=8; 0x0f: [12]=0, [15:13]=7, 0x1f: [0:4]=19
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x40); 
                    set_blake_conf(mcb, addr, (mcb->blake_conf & 0xffffff00) | 0xc8);
                    set_hash_conf(mcb, addr, (mcb->hash_conf & 0xffffefff) | 0xe000L);
                    set_nonce_pos_ex(mcb, addr, (mcb->nonce_pos_ex & 0xfffffffe0) | (19L << 0));
                }
                break;
            case ASIC_MIDDLE_5: 
                if (algo == ALGO_LYRA2REV2)     // 0x10: [7]=1; 0x0f: [16]=0, [19:17]=7, 0x1f: [5:9]=3
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x80);  
                    set_hash_conf(mcb, addr, (mcb->hash_conf & 0xfffeffff) | 0xe0000L);
                    set_nonce_pos_ex(mcb, addr, (mcb->nonce_pos_ex & 0xfffffc1f) | (3L << 5));
                }
                break;  
                
            case ASIC_COMB: // middle 6
                comb_config(fd_uart, addr, algo, &mid_cb[chip_index]);
                comb_first_config++;
                break;
                
            case ASIC_MIDDLE_7: // xdag (refer to xdag miner), , 0x1e: [20:24]=3
                break;

            case ASIC_MIDDLE_8:
                if (algo == ALGO_LYRA2Z)        // 0x10: [8]=1; 0x0f: [28]=0,[29:31]=7, 0x1e: [26:30]=3; 0x09: [19]=1, 0x0d: [4]=1
                {
                    set_clock_gating(mcb, addr, mcb->clock_gating | 0x100L);  
                    set_share_algo(mcb, addr, mcb->share_algo | 0x00000010);
                    set_hash_conf(mcb, addr, (mcb->hash_conf & 0xefffffff) | 0xe0000000L);
                    set_nonce_pos(mcb, addr, (mcb->nonce_pos & 0x83ffffff) | (3L << 26));
                    set_comb_subclk(mcb, addr, mcb->comb_sub_clk | (1L << 19));
                }
                break;
            
            default:
                break;
            }

            index++;
        }
    }

    for (int i = 0; i < 256; i++)
    {
        if (mid_cb[i].reg_permutation[0].reg != 0)
            send_reg_permutation(fd_uart, &mid_cb[i]);
        else
            break;
    }
}

static size_t trigg_curl_write(void *src, size_t size, size_t nmemb, void *dst)
{
    int len = size*nmemb;
    memcpy(dst, src, len);
    applog(LOG_DEBUG, "getwork = '%s'", dst);

    return len;
}



static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

static char *decoding_table = NULL;
static int mod_table[] = {0, 2, 1};

void build_decoding_table() {
    decoding_table = malloc(256);

    for (int i = 0; i < 64; i++)
        decoding_table[(unsigned char) encoding_table[i]] = i;
}

char *base64_encode(const unsigned char *data, size_t input_length, size_t *output_length) {

    *output_length = 4 * ((input_length + 2) / 3);

    char *encoded_data = malloc(*output_length);
    if (encoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[*output_length - 1 - i] = '=';

    return encoded_data;
}


unsigned char *base64_decode(const char *data, size_t input_length, size_t *output_length) {
    if (decoding_table == NULL) build_decoding_table();

    if (input_length % 4 != 0) return NULL;

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    unsigned char *decoded_data = malloc(*output_length);
    if (decoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }

    return decoded_data;
}

void base64_cleanup() {
    free(decoding_table);
}

static void *getwork_thread(void *userdata)
{
	CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    char *rpc_url = miner_cb[0].rpc_url;
    char *rpc_user = miner_cb[0].rpc_user;
    char buf[256] = {0};
    int connected = 0;

    char work_str[128] = {0};
    char work_str_last[128] = {0};
    uint32_t broot[8];

    uint32_t diff, nblock;

    json_t *jval, *jbroot, *jdiff, *jnblock;
    json_error_t err;

    struct stratum_ctx *sctx = &stratum_v[0];

    const char REQ_TRIGG[] = "{\"r\" : \"mining\"}";
    const char UA_TRIGG[] = "User-Agent: amm-mcm";

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, rpc_url);

    //curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    //curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 15L);    // < 30s
    //curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 3L);

    headers = curl_slist_append(headers, UA_TRIGG);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);            // 30s
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, REQ_TRIGG);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, trigg_curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

    for (;;) {
        memset(buf, 0, sizeof(buf));
        CURLcode res = curl_easy_perform(curl);
        if (CURLE_OK != res) {
            applog(LOG_ERR, "curl_easy_perform() fail: %d\n", res);
            curl_easy_cleanup(curl);
            if (connected)
                proper_exit(ERR_POOL_DISCONNECTED);
            else
                proper_exit(ERR_POOL_UNREACHABLE);
        } else {
            connected = 1;
            if ((jval = JSON_LOADS(buf, &err)) != NULL) {
                // broot
                jbroot = json_object_get(jval, "broot");
                strcpy(work_str, json_string_value(jbroot));
                // diff
                jdiff = json_object_get(jval, "diff");
                diff = json_integer_value(jdiff);
                // nblock
                jnblock = json_object_get(jval, "nblock");
                nblock = json_integer_value(jnblock);
                json_decref(jval);

                if (jbroot && jdiff && jnblock) {
                    if (strcmp(work_str_last, work_str) != 0) {
                        strcpy(work_str_last, work_str);
                        applog(LOG_BLUE, "get new work %d", nblock);

                        size_t root_len;
                        unsigned char *bres = base64_decode(work_str, strlen(work_str), &root_len);
                        memcpy(broot, bres, 32);
                        free(bres);

                        // copy work to global statum_ctx
                        pthread_mutex_lock(&sctx->work_lock);
					    time(&g_work_time_v[0]);
                        sctx->nblock = nblock;
                        sctx->tdiff = diff;
                        memcpy(sctx->broot, broot, 32);
                        pthread_mutex_unlock(&sctx->work_lock);

                        applog(LOG_DEBUG, "base64 decode: length %d", root_len);
                        for (int i=0; i<8; i++) {
                            applog(LOG_DEBUG, "broot[%d]: 0x%08x", i, broot[i]);
                        }
                    }
                } else {
                    applog(LOG_ERR, "JSON decode failed(%d): %s", err.line, err.text);
                }
            }
        }

        sleep(3);
    }
}

int main(int argc, char *argv[]) {
	struct thr_info *thr;
	long flags;
	int i, err;

    time(&hash_err_time);

	for (int j = 0; j < MAX_POOL_NUM; j++)
	{
	    miner_cb[j].have_gbt = true;
	    miner_cb[j].opt_algo = 0;
	    miner_cb[j].rpc_pass = 0;
	    miner_cb[j].rpc_url = 0;
	    miner_cb[j].rpc_user = 0;
	    miner_cb[j].rpc_userpass = 0;
	    miner_cb[j].short_url = 0;

	    miner_cb[j].opt_diff_factor = 1.0;
	}

	pthread_mutex_init(&applog_lock, NULL);
	pthread_mutex_init(&miner_chain_mutex, NULL);
	time(&startup_time); 

	print_arg(argc, argv);
	host_type = get_host_type(); 

	show_credits();
	device_name = strdup("/dev/ttyAMA0");
    for (int i = 0; i < MAX_POOL_NUM; i++)
    {
	    miner_cb[i].rpc_user = strdup("");
	    miner_cb[i].rpc_pass = strdup("");
    }
	opt_api_allow = strdup("0.0.0.0"); /* 0.0.0.0 for all ips */

#if defined(WIN32)
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	num_cpus = sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_CONF)
	num_cpus = sysconf(_SC_NPROCESSORS_CONF);
#elif defined(CTL_HW) && defined(HW_NCPU)
	int req[] = { CTL_HW, HW_NCPU };
	size_t len = sizeof(num_cpus);
	sysctl(req, 2, &num_cpus, &len, NULL, 0);
#else
	num_cpus = 1;
#endif
	if (num_cpus < 1)
		num_cpus = 1;

    fpga_reset_init();

	/* parse command line */
	parse_cmdline(argc, argv);
    if (opt_url_cnt != opt_algo_cnt) {
        applog(LOG_ERR, "argument error: url not dismatch algo");
        exit(1);
    }

    // not use
    /*
	if (!opt_benchmark && !rpc_url) {
		// try default config file in binary folder
		char defconfig[MAX_PATH] = { 0 };
		get_defconfig_path(defconfig, MAX_PATH, argv[0]);
		if (strlen(defconfig)) {
			if (opt_debug)
				applog(LOG_DEBUG, "Using config %s", defconfig);
			parse_arg('c', defconfig);
			parse_cmdline(argc, argv);
		}
	}
    */

	fd_uart = Dev_Init(device_name);
	if (fd_uart == -1)
	{
		applog(LOG_ERR, "Open %s failed", device_name);
		proper_exit(ERR_UART_OPEN);
	}
	fpga_write_reg(fd_uart, 0, 0x32, 0x0000ae4e);   // disable tmeper alarm
	sleep(1);
	
    tcflush(fd_uart, TCIOFLUSH);

	if (!opt_n_threads)
		opt_n_threads = 1;

    /*
	if (opt_algo == ALGO_QUARK) {
		init_quarkhash_contexts();
	} else if(opt_algo == ALGO_CRYPTONIGHT || opt_algo == ALGO_CRYPTOLIGHT) {
		jsonrpc_2 = true;
		opt_extranonce = false;
		aes_ni_supported = has_aes_ni();
		if (!opt_quiet) {
			applog(LOG_INFO, "Using JSON-RPC 2.0");
			applog(LOG_INFO, "CPU Supports AES-NI: %s", aes_ni_supported ? "YES" : "NO");
		}
	} else if(opt_algo == ALGO_DECRED || opt_algo == ALGO_SIA) {
		have_gbt = false;
	}
    */
    for (int i = 0; i < opt_algo_cnt; i++)
    {
        miner_cb[i].have_gbt = true;   // default value

        if (miner_cb[i].opt_algo == ALGO_DECRED || miner_cb[i].opt_algo == ALGO_SIA) {
		    miner_cb[i].have_gbt = false;
        }
    }

    for (int i = 0; i < opt_url_cnt; i++)
    {
        if (!opt_benchmark && !miner_cb[i].rpc_url) {
            fprintf(stderr, "%s: no URL supplied\n", argv[0]);
            show_usage_and_exit(1);
        }
    }

    for (int i = 0; i < opt_url_cnt; i++)
    {
        if (!miner_cb[i].rpc_userpass) {
            miner_cb[i].rpc_userpass = (char*) malloc(strlen(miner_cb[i].rpc_user) + strlen(miner_cb[i].rpc_pass) + 2);
            if (!miner_cb[i].rpc_userpass)
                return 1;
            sprintf(miner_cb[i].rpc_userpass, "%s:%s", miner_cb[i].rpc_user, miner_cb[i].rpc_pass);
        }
    }

	pthread_mutex_init(&stats_lock, NULL);
    for (int i = 0; i < opt_url_cnt; i++)
    {
	    pthread_mutex_init(&g_work_lock_v[i], NULL);
    }
	pthread_mutex_init(&rpc2_job_lock, NULL);
	pthread_mutex_init(&rpc2_login_lock, NULL);
    
    for (int i = 0; i < opt_url_cnt; i++) {
	    pthread_mutex_init(&stratum_v[i].sock_lock, NULL);
	    pthread_mutex_init(&stratum_v[i].work_lock, NULL);
    }

    // all pool use the same protocal
	flags = !opt_benchmark && strncmp(miner_cb[0].rpc_url, "https:", 6)
	        ? (CURL_GLOBAL_ALL & ~CURL_GLOBAL_SSL)
	        : CURL_GLOBAL_ALL;
	if (curl_global_init(flags)) {
		applog(LOG_ERR, "CURL initialization failed");
		return 1;
	}

	if (opt_background) {
		i = fork();
		if (i < 0) exit(1);
		if (i > 0) exit(0);
		i = setsid();
		if (i < 0)
			applog(LOG_ERR, "setsid() failed (errno = %d)", errno);
		i = chdir("/");
		if (i < 0)
			applog(LOG_ERR, "chdir() failed (errno = %d)", errno);
		signal(SIGHUP, signal_handler);
		signal(SIGTERM, signal_handler);
	}

	/* Always catch Ctrl+C */
	signal(SIGINT, signal_handler);

	if (opt_affinity != -1) {
		if (!opt_quiet)
			applog(LOG_DEBUG, "Binding process to cpu mask %x", opt_affinity);
		affine_to_cpu_mask(-1, (unsigned long)opt_affinity);
	}

#ifdef HAVE_SYSLOG_H
	if (use_syslog)
		openlog("cpuminer", LOG_PID, LOG_USER);
#endif

	thr_info = (struct thr_info*) calloc(2 + opt_url_cnt*2, sizeof(*thr));
	if (!thr_info)
		return 1;

	/* init workio thread info */
    for (int i = 0; i < opt_url_cnt; i++) {
        work_thr_id = i;
        thr = &thr_info[work_thr_id];
        thr->id = work_thr_id;
        thr->q = tq_new();
        thr->res_id = i;
        if (!thr->q)
            return 1;

        /* start work I/O thread */
        if (thread_create(thr, workio_thread)) {
            applog(LOG_ERR, "work thread create failed");
            return 1;
        }
    }
	//if (rpc_pass && rpc_user)
	//	opt_stratum_stats = (strstr(rpc_pass, "stats") != NULL) || (strcmp(rpc_user, "benchmark") == 0);

    pthread_t tid_getw;
	if (miner_cb[0].opt_algo == ALGO_TRIGG) {
        want_longpoll = false;
        if (pthread_create(&tid_getw, 0, getwork_thread, 0)) {
            applog(LOG_ERR, "getwork thread create failed");
            return 1;
        }
    }

	/* ESET-NOD32 Detects these 2 thread_create... */
	if (want_longpoll && !have_stratum) {   // logpoll not use
		/* init longpoll thread info */
		longpoll_thr_id = opt_url_cnt * 2;  // after 'workio' & 'stratum' thread
		thr = &thr_info[longpoll_thr_id];
		thr->id = longpoll_thr_id;
		thr->q = tq_new();
		if (!thr->q)
			return 1;

		/* start longpoll thread */
		err = thread_create(thr, longpoll_thread);
		if (err) {
			applog(LOG_ERR, "long poll thread create failed");
			return 1;
		}
	}

    // init and start stratum thread
    for (int i = 0; i < opt_url_cnt; i++) {
        if (want_stratum) {
            /* init stratum thread info */
            stratum_thr_id = i + opt_url_cnt;
            thr = &thr_info[stratum_thr_id];
            thr->id = stratum_thr_id;
            thr->q = tq_new();
            thr->res_id = i;
            if (!thr->q)
                return 1;

            /* start stratum thread */
            err = thread_create(thr, stratum_thread);
            if (err) {
                applog(LOG_ERR, "stratum thread create failed");
                return 1;
            }
            if (have_stratum)
                tq_push(thr_info[stratum_thr_id].q, strdup(miner_cb[i].rpc_url));
        }
    }

	broadcast_clear_chip(fd_uart);
	sleep(1);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_t   pth_upstream;
	pthread_create(&pth_upstream, &attr, fpga_upstream_thread, &fd_uart);
	pthread_attr_destroy(&attr);

    if (opt_asic_mode)
    {
        for (int j = 0; j < 256; j++)
        {
            init_middle_cb(&mid_cb[j]);
        }
        middles_config();
	}
	
	if (opt_chip_num > 0)
	{
		if (opt_asic_mode == false)
		{
			miner_chip_count = opt_chip_num;
			for (i = 1; i <= miner_chip_count; i++) 
			{
				create_miner(i, 0xffffffffu, -1);
			}
		}
		else
		{
			miner_chip_count = opt_chip_num;
            for (int j = 0; j < opt_algo_cnt; j++)
            {
                int index = 0;
                while (middle_stage[j][index])
                {
                    if (0 == check_chip_algo(middle_stage[j][index], miner_cb[j].opt_algo))
                    {
                        create_miner(middle_stage[j][index], 0xffffffffu, j);
                    }
                    else
                    {
                        applog(LOG_WARNING, "chip algo '%s' not support!", algo_names[miner_cb[j].opt_algo]);
                    }
                    index++;
                }
            }
		}
	}
	else 
	{
		applog(LOG_INFO, "auto search chip");
		broadcast_chip_msg(fd_uart);
		sleep(1);
	}

	sleep(2);

	if (miner_chip_count == 0)
	{
		applog(LOG_ERR, "NO chip found, exit");
		proper_exit(ERR_CHIP_TIMEOUT);
	}

	if (opt_chip_num > 0)
	{
		broadcast_chip_msg(fd_uart);
		sleep(1);
	}

	if (opt_api_listen) {
		/* api thread */
		api_thr_id = 3;
		thr = &thr_info[api_thr_id];
		thr->id = api_thr_id;
		thr->q = tq_new();
		if (!thr->q)
			return 1;
		err = thread_create(thr, api_thread);
		if (err) {
			applog(LOG_ERR, "api thread create failed");
			return 1;
		}
	}

	pthread_t   pth_guard;
	pthread_attr_init(&attr);
	pthread_create(&pth_guard, &attr, miner_guard_thread, NULL);
	pthread_attr_destroy(&attr);

	opt_n_threads = miner_chip_count; 

    char str_algo[MAX_POOL_NUM * 16];
    int str_algo_len = 0;
    for (int i = 0; i < opt_url_cnt; i++)
    {
        str_algo_len += sprintf(str_algo+str_algo_len, "%s & ", algo_names[miner_cb[i].opt_algo]);
    }
    if (str_algo_len > 0)
        str_algo[str_algo_len - 3] = '\0';

    applog(LOG_NOTICE, "%d miner started, "
        "using '%s' algorithm.",
        miner_chip_count, str_algo);

	/* main loop - simply wait for workio thread to exit */
	pthread_join(thr_info[work_thr_id].pth, NULL);

    proper_exit(ERR_POOL_DISCONNECTED);
	applog(LOG_WARNING, "workio thread exit.");

	return 0;
}

