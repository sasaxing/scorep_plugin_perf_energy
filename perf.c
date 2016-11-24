// estimation equation: Joules.power.energy.cores = 1.602416e-9*instructions-9.779874e-11 *cpu.cycles +  6.437730e-08*cache.misses +2.418160e+03

///////////////////////////////// start： headers///////////////////////////////////////////
// check if there exists VT/SCOREP，then decide which header to include.
// in my case, when building: -DSCOREP_DIR=~/install/scorep, so scorep detected->scorep_MetricPlugin.h
 
#ifdef SCOREP
#include <scorep/SCOREP_MetricPlugins.h>
#else
#error "You need Score-P  to compile this plugin"
#endif /* SCOREP*/
 

#include <inttypes.h>  //PRIu64
#include <stdlib.h>   // strtoll();   malloc();
#include <string.h>
#include <stdio.h>
#include <errno.h>   
#include <unistd.h>  
#include <stdint.h>  //uint64_t
#include <sys/syscall.h>

#include <linux/perf_event.h>
////////////////////////////// end： headers////////////////////////////////////////////


////////////////////////////// start : global variables. ////////////////////////////// 
#define N 3  //  how many counters I use for estimation. CTNAME_FD ctname_fd[N]

/* define the unique id for metrics (add_counter) */
#define ENERGY_THREAD 1 

// mnemonic for base perf event-counters. why cannot '-'
enum perf_event{
  cpu_cycles=1,
  cycles=1,
  instructions=2,
  cache_references=3,
  cache_misses=4,
  branch_instructions=5,
  branches=5,
  branch_misses=6
};

typedef struct
{
  int event_num;
  int fd;
}CTNAME_FD;

// global array for fd setter and getter.
// store those that may be useful to calculate required metric result.
static CTNAME_FD ctname_fd[N]={
  {instructions,-1},
  {cpu_cycles,-1},
  {cache_misses,-1}
};  

////////////////////////////// end : global variables. ////////////////////////////// 


/* init and fini do not do anything */
/* This is intended! */
int32_t init(){
  return 0;
}
void fini(){
  /* we do not close perfs file descriptors */
  /* as we do not store this information */
}

/////////////////begin: assign value to all attr.type , attr.config.//////////////////////

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

    default:
      break;

    }

}

/////////////////end: assign value to all attr.type , attr.config.////////////////////


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
  fd = perf_event_open(&attr, 0,-1,-1,0);
  for(i=0;i<N;i++){
    if(ctname_fd[i].event_num == event_num){
      ctname_fd[i].fd = fd;
    }
  }
}

/* registers perf event , it provides a unique ID for each metric.*/
int32_t add_counter(char * event_name)
{
  int id=0; //unique id.

  int i=0; // for loop.

  if(strstr( event_name, "energy-thread" ) == event_name)
  {
    id = ENERGY_THREAD;

    set_fd(instructions);
    set_fd(cpu_cycles);
    set_fd(cache_misses);

    return id;
  }
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

  if(fd<=1){
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
  if(fd <= 0){
      fprintf(stderr, "Unable to get event for event_num=%d!",event_num);
      return -1;
  }

  ret =read(fd, &count, sizeof(uint64_t));

  if (ret!=sizeof(uint64_t)){
    return -1;
  }

  return count;
}


uint64_t get_value(int id){
  uint64_t result;
  
  uint64_t count1=0; 
  uint64_t count2=0; 
  uint64_t count3=0;


  if(id == ENERGY_THREAD){  // required event: energy-cores = 0.1*cpu-cycles + 10*cache-misses.
    
    count1=get_counterValue(instructions);
    count2=get_counterValue(cpu_cycles);
    count3=get_counterValue(cache_misses);

    if (count1<0|| count2<0 || count3<0){
      return !0;
    }
     
  }
  
  result = 1.602416e-9*count1-9.779874e-11 *count2 +  6.437730e-08*count3 +2.418160e+03;
 
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
  return_values[0].unit        = NULL;
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
    SCOREP_Metric_Plugin_Info info;
    memset( &info, 0, sizeof( SCOREP_Metric_Plugin_Info ) );

    /* Set up the structure */
    info.plugin_version               = SCOREP_METRIC_PLUGIN_VERSION;
    info.run_per                      = SCOREP_METRIC_PER_THREAD;
    info.sync                         = SCOREP_METRIC_SYNC;
    info.initialize                   = init;
    info.finalize                     = fini;
    info.get_event_info               = get_event_info;
    info.add_counter                  = add_counter;
    info.get_current_value            = get_value;
    info.get_optional_value           = get_optional_value;

    return info;
}

#endif /* SCOREP */
