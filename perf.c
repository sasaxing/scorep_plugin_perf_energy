// version14: 
// using global variables to distribute energy to threads
// ENERGY-THREAD = m_i/m_sum * ENERGY-CORES

/* Comparision to previous versions: 
 * 
 * version 12: contains false sharing
 * version 14: fix false sharing
 *
 */


// m_i : estimation equation: Joules.power.energy.cores = 1.602416e-9*instructions-9.779874e-11 *cpu.cycles +  6.437730e-08*cache.misses +2.418160e+03

///////////////////////////////// start： headers///////////////////////////////////////////
// check if there exists VT/SCOREP，then decide which header to include.
// in my case, when building: -DSCOREP_DIR=~/install/scorep, so scorep detected->scorep_MetricPlugin.h
 
#ifdef SCOREP
#include <scorep/SCOREP_MetricPlugins.h>
#else
#error "You need Score-P  to compile this plugin"
#endif /* SCOREP*/
 
#include <omp.h>
#include <inttypes.h>  //PRIu64
#include <stdlib.h>   // strtoll();   malloc();
#include <string.h>
#include <stdio.h>
#include <errno.h>   
#include <unistd.h>  
#include <stdint.h>  //uint64_t
#include <sys/syscall.h>
#include <sys/types.h>

#include <linux/perf_event.h>
////////////////////////////// end： headers////////////////////////////////////////////


////////////////////////////// start : global variables. ////////////////////////////// 
#define N 4  //  how many counters I use for estimation. CTNAME_FD ctname_fd[N]

/* define the unique id for metrics (add_counter) */
#define ENERGY_THREAD 1 //first metric, m_i  --> the portion of E for each thread
#define POWER_ENERGY_CORES 2   // second metric. E-cores --> total E.

/* define type, config value for RAPL attr*/
#define PERF_TYPE_RAPL 10
#define PERF_COUNT_ENERGY_COERS 1
#define PERF_COUNT_ENERGY_PKG 2
#define PERF_COUNT_ENERGY_RAM 3
#define PERF_COUNT_ENERGY_GPU 4
#define PERF_RAPL_SCALE 2.3283064365386962890625e-10 // unit: Joules
//#define PERF_RAPL_SCALE 2.3283064365386962890625e-1 // unit: Nanojoules

#define N_THREADS 2  //must be constant, compiler needs to assign space for arrays.

#define CACHE_LINE_SIZE 64

struct thread_local {
  uint64_t m_i;
} __attribute__ ((aligned(CACHE_LINE_SIZE)));

struct thread_local local_val[N_THREADS];

// mnemonic for base perf event-counters. 
enum perf_event{
  cpu_cycles=1,
  cycles=1,
  instructions=2,
  cache_references=3,
  cache_misses=4,
  branch_instructions=5,
  branches=5,
  branch_misses=6,
  power_energy_cores=21   // need sudo
};

typedef struct
{
  int event_num;
  int fd;
}CTNAME_FD;

// global array for fd setter and getter.
// store those that are useful to calculate required metric result.
static CTNAME_FD ctname_fd[N]={
  {instructions,-1},
  {cpu_cycles,-1},
  {cache_misses,-1},
  {power_energy_cores,-1}
};  

////////////////////////////// end : global variables. ////////////////////////////// 

/* init() is intended, executed only once at the beginning before getting any result.*/
int32_t init(){
  int i=0;
  for(i=0;i<N_THREADS;i++){
    local_val[i].m_i=0;
//    local_val[i].round_num=0;
  }
  return 0;
}

//This functions is called once per process to clean up all resources used by the metric plugin.
void fini(){
  /* we do not close perfs file descriptors */
  /* as we do not store this information */
}

/* This function writes the attr definitions for a given event name
 * If the event has not been found, attr->type is PERF_TYPE_MAX
 * */
void build_perf_attr(struct perf_event_attr * attr, int event_num)
{
  
  memset( attr, 0, sizeof( struct perf_event_attr ) ); 
  //The first n bits of attr will be replaced by n 0s. n = sizeof( struct perf_event_attr).
  attr->config1 = 0;
  attr->config2 = 0;
  attr->type    = PERF_TYPE_MAX;


  // those that will be used.
  switch(event_num){
    case cpu_cycles:
      attr->type   = PERF_TYPE_HARDWARE;
      attr->config = PERF_COUNT_HW_CPU_CYCLES;
      break;

    case instructions:
      attr->type   = PERF_TYPE_HARDWARE;
      attr->config = PERF_COUNT_HW_INSTRUCTIONS;
      break;

    case cache_references:
      attr->type   = PERF_TYPE_HARDWARE;
      attr->config = PERF_COUNT_HW_CACHE_REFERENCES;
      break;

    case cache_misses:
      attr->type   = PERF_TYPE_HARDWARE;
      attr->config = PERF_COUNT_HW_CACHE_MISSES;
      break;

    case branch_instructions:
      attr->type   = PERF_TYPE_HARDWARE;
      attr->config = PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
      break;

    case branch_misses:
      attr->type   = PERF_TYPE_HARDWARE;
      attr->config = PERF_COUNT_HW_BRANCH_MISSES;
      break;

    case power_energy_cores: 
      attr->type   = PERF_TYPE_RAPL;
      attr->config = PERF_COUNT_ENERGY_COERS;
      break;      

    default:
      break;

    }

}


/* syscall to gather performance counters. */
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                       int cpu, int group_fd, unsigned long flags)
{
  int ret;
  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
  return ret;
}

void set_fd(int event_num)
{
  int i;
  int fd;
  struct perf_event_attr attr; 

  build_perf_attr(&attr, event_num);

  if(event_num == power_energy_cores){ //rapl-read
    fd = perf_event_open(&attr, -1,0,-1,0);
  }else{ //others
    fd = perf_event_open(&attr, 0,-1,-1,0);
  }
  
  for(i=0;i<N;i++){
    if(ctname_fd[i].event_num == event_num){
      ctname_fd[i].fd = fd;
    }
  }
}

/* registers perf event , it provides a unique ID for each metric.*/
int32_t add_counter(char * event_name)  // callback, fixed parameter : metric name(string)--> not possible to use switch.
{
  int id=0; //unique id.

  int i=0; // for loop.

  if(strstr( event_name, "energy-thread" ) == event_name)
  {
    id = ENERGY_THREAD;

    set_fd(instructions);
    set_fd(cpu_cycles);
    set_fd(cache_misses);
    set_fd(power_energy_cores);

  }else if(strstr( event_name, "power-energy-cores" ) == event_name){
    
    id = POWER_ENERGY_CORES;

    set_fd(power_energy_cores);

  }

  return id;
}

/* reads value repeatedly */

int get_fd(int event_num)
{

  int i=0;
  struct perf_event_attr attr; 
  int fd=-1;

  build_perf_attr(&attr, event_num);
  for(i=0;i<N;i++){
    if(ctname_fd[i].event_num == event_num){
      fd=ctname_fd[i].fd;
    }
  }

  if(fd<=-1){
      fprintf(stderr, "Error: Failed to get counter %d!\n", event_num);
      return -1;
  }

  return fd;
}

uint64_t get_counterValue(int event_num){

  int fd;
  size_t ret;
  uint64_t count;

  fd = get_fd(event_num);
  if(fd <= -1){
      fprintf(stderr, "Unable to get event for event_num=%d!\n",event_num);
      return -1;
  }

  ret =read(fd, &count, sizeof(uint64_t));

  if (ret!=sizeof(uint64_t)){
    return -1;
  }

  return count;
}

uint64_t get_value(int id){
  int i;
  uint64_t result;
  
  uint64_t count1=0; 
  uint64_t count2=0; 
  uint64_t count3=0;
  uint64_t energy_cores_value=0;

  uint64_t m_sum=0;

  int tid;
  tid=omp_get_thread_num();


  if(id == ENERGY_THREAD){ 
    // 0. read all values.
    count1=get_counterValue(instructions);
    count2=get_counterValue(cpu_cycles);
    count3=get_counterValue(cache_misses);
    energy_cores_value=get_counterValue(power_energy_cores) * PERF_RAPL_SCALE;
    if (count1<0|| count2<0 || count3<0 || energy_cores_value<0){
      return !0;
    }
 
    local_val[tid].m_i = 1.602416e-9*count1 - 9.779874e-11*count2 + 6.437730e-08*count3 + 2.418160e+03;  // record the current measurement.

    for(i=0;i<N_THREADS;i++){
      m_sum += local_val[tid].m_i;
    }

    result = local_val[tid].m_i*(1.0)/m_sum * energy_cores_value;
  

  }else if(id == POWER_ENERGY_CORES) {
    energy_cores_value = get_counterValue(power_energy_cores) * PERF_RAPL_SCALE;
    if(energy_cores_value < 0){
      return !0;
    }

    result = energy_cores_value ;

  }

  return result;
}

#ifdef SCOREP

SCOREP_Metric_Plugin_MetricProperties * get_event_info(char * event_name)
{
  SCOREP_Metric_Plugin_MetricProperties * return_values;
  uint64_t id = add_counter(event_name);

    /* wrong metric */
  if (id < 0){
    fprintf(stderr, "PERF metric not recognized: %s", event_name );
    return NULL;
  }
  return_values= malloc(2 * sizeof(SCOREP_Metric_Plugin_MetricProperties) );   /// why 2????
  if (return_values==NULL){
        fprintf(stderr, "Score-P Perf Plugin: "
                "failed to allocate memory for passing information to Score-P.\n");
        return NULL;
  }
  return_values[0].name        = strdup(event_name);  //strdup == duplicate.
  return_values[0].unit        = "Joules";
  return_values[0].description = NULL;
  return_values[0].mode        = SCOREP_METRIC_MODE_ACCUMULATED_START;
  return_values[0].value_type  = SCOREP_METRIC_VALUE_UINT64;
  return_values[0].base        = SCOREP_METRIC_BASE_DECIMAL;
  return_values[0].exponent    = 0;
  return_values[1].name=NULL;
  return return_values;
}

bool get_optional_value( int32_t id, uint64_t* value ){
  *value=get_value(id);
  return true;
}

/**
 * This function get called to give some informations about the plugin to scorep
 */

// define the name of this plugin, with its inner structure.
SCOREP_METRIC_PLUGIN_ENTRY( perf_plugin )
{
    /* Initialize info data (with zero) */
  //SCOREP_Metric_Plugin_Info :  defined as a struct in SCOREP_MetricPlugins.h
    SCOREP_Metric_Plugin_Info info;
    memset( &info, 0, sizeof( SCOREP_Metric_Plugin_Info ) );

    /* Set up the structure */
    info.plugin_version               = SCOREP_METRIC_PLUGIN_VERSION;  // uint32_t
    info.run_per                      = SCOREP_METRIC_PER_THREAD; // <SCOREP_MetricTypes.h>: SCOREP_MetricPer, int enum
    info.sync                         = SCOREP_METRIC_SYNC; //<SCOREP_MetricTypes.h>:SCOREP_MetricSynchronicity, int enum
    //info.delta_t                      = 1;  //uint64_t, default=0; Set a specific interval for reading metric values.
    info.initialize                   = init;   // function as a member: int32_t(void)
    info.finalize                     = fini;   // function as a member: void(void)
    info.get_event_info               = get_event_info;   // ditto: SCOREP_Metric_Plugin_MetricProperties(char* token)
    info.add_counter                  = add_counter;      // ditto: int32_t(char* metric_name)
    info.get_current_value            = get_value;        // ditto: uint64_t(int32_t id)
    info.get_optional_value           = get_optional_value;   // ditto: bool(int32_t id, uint_64* value)
    //some other members in this SCOREP_Metric_Plugin_Info Struct but useless here 
    //info.set_clock_function;   // ditto: void(uint64_t)
    //info.get_all_values;    //ditto: uint64_t(int32_t, SCOREP_MetricTimeValuePair**)
    //info.synchronize;         //ditto: void(bool,SCOREP_MetricSynchronizationMode)
    
    //info.reserved;        //Reserved space for future features, should be zeroed 

    return info;
}

#endif /* SCOREP */

