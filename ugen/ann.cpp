#include "pool.hpp"
#include "fann_rt.hpp"

#include <SC_PlugIn.h>
#include <cstring>

#define IN_ANN_NUM 0
#define IN_OUT_COUNT 1
//#define IN_INPUTS 2
#define IN_SIG 2
#define IN_PERIOD 3

// InterfaceTable contains pointers to functions in the host (server).
InterfaceTable *ft;

// declare struct to hold unit generator state
struct Ann : public Unit
{
  struct fann_rt *ann;
  unsigned int inc;
  unsigned int outc;
  fann_type *inputs;
  int bufsz;
  float *buf;
  int bufph;
  float bufmin;
  float bufmax;
  float avgFactor;
  float *out;
#if 0
  // for purpose of getting buffer data
  float m_fbufnum;
  SndBuf *m_buf;
#endif
};


// declare unit generator functions
static void Ann_Ctor(Ann* unit);
static void Ann_Dtor(Ann* unit);
static void Ann_idle(Ann *unit, int inNumSamples);
static void Ann_next(Ann *unit, int inNumSamples);


PluginLoad(Ann)
{
    // InterfaceTable *inTable implicitly given as argument to the load function
    ft = inTable;

    initPool();
    DefineDtorUnit(Ann);
}

void Ann_Ctor(Ann* unit)
{
  unit->ann = 0;
  unit->inc = 0;
  unit->outc = 0;
  unit->inputs = 0;
  unit->bufsz = 0;
  unit->buf = 0;
  unit->bufph = 0;
  unit->bufmin = 0.0;
  unit->bufmax = 0.0;
  unit->avgFactor = 0.0;
  unit->out = 0;

  int annNum = IN0( IN_ANN_NUM );
  if( annNum < 0 || annNum > kAnnDefCount ) {
    Print("ANN index out of range: %i\n", annNum);
    SETCALC( Ann_idle );
    return;
  }
  Print("Ann index ok: %i\n", annNum );

  AnnDef *annDef = annDefs[annNum];
  if( !annDef ) {
    Print("No ANN loaded at index %i\n", annNum);
    SETCALC( Ann_idle );
    return;
  }
  Print("Got Ann.\n", annNum );

  unit->ann = fann_rt_create( annDef->_ann, ft, unit->mWorld );
  if( !unit->ann ) {
    Print("Could not create RT ANN\n");
    SETCALC( Ann_idle );
    return;
  }
  Print("RT ANN created\n");

  unit->inc = unit->ann->num_input; //fann_get_num_input( unit->ann );
  unit->outc = unit->ann->num_output; //fann_get_num_output( unit->ann );

  if( IN0(IN_OUT_COUNT) != unit->outc ) {
    Print( "Error: Output count mismatch!\n" );
    SETCALC( Ann_idle );
    return;
  }
  else Print( "ANN struct OK.\n" );

    // print structure
#if 0
  unsigned int layer_c = fann_get_num_layers( unit->ann );
  unsigned int *layer_v = (unsigned int*) RTAlloc( unit->mWorld, sizeof(unsigned int) * layer_c );
  fann_get_layer_array( unit->ann, layer_v );

  for( int i=0; i < layer_c; ++i ){
    Print("%i ", layer_v[i]);
  }
  Print("\n");
  RTFree( unit->mWorld, layer_v );
#endif

#if 0
  fann_layer *layer;
  for( layer = unit->ann->first_layer; layer != unit->ann->last_layer; ++layer )
  {
    Print("%i ", layer->last_neuron - layer->first_neuron - 1);
  }
  Print("\n");
#endif

  // init ANN input buffer

  unit->inputs = (fann_type*) RTAlloc( unit->mWorld, sizeof(fann_type) * unit->inc );
  for(int i=0; i < unit->inc; i++) unit->inputs[i] = 0.f;

  // init signal input buffer

  int period = IN0( IN_PERIOD );
  unit->bufsz = period > 1 ? period : 1;
  unit->avgFactor = 1.f / unit->bufsz;
  unit->buf = (float*) RTAlloc( unit->mWorld, sizeof(float) * unit->bufsz );
  //Print("bufsz %i avgFactor %f\n",unit->bufsz, unit->avgFactor);

  // init signal output buffers (note: 1 sample per "buffer" only)

  unit->out = (float*) RTAlloc( unit->mWorld, sizeof(float) * unit->outc );
  for(int i=0; i < unit->outc; i++) {
    unit->out[i] = 0.f;
  }

  SETCALC( Ann_next);

  // Calculate one sample of output
  Ann_next( unit, 1 );
  //Ann_idle( unit, 1 );
}

void Ann_Dtor(Ann* unit)
{
  Print("Destroying Ann UGen\n");
  fann_rt_destroy( unit->ann, ft, unit->mWorld );
  RTFree( unit->mWorld, unit->inputs );
  RTFree( unit->mWorld, unit->buf );
  RTFree( unit->mWorld, unit->out );
}

void Ann_idle(Ann *unit, int inNumSamples)
{
  float *out = OUT(0);
  //for( int i = 0; i < inNumSamples; ++i ) {
    out[0] = 0.f;
  //}
}

void Ann_next(Ann *unit, int numSamples)
{
  int ph = unit->bufph;
  float in = IN0(IN_SIG);
  unit->buf[ ph ] = in;
  unit->bufmin += in;
  /*if( ph > 0 ) {
    if( in > unit->bufmax ) unit->bufmax = in;
    else if( in < unit->bufmin ) unit->bufmin = in;
  }
  else {
    unit->bufmin = unit->bufmax = in;
  }*/
  ph++;
  if( ph >= unit->bufsz ) {
    ph = 0;

    // calc average in of period and push to the end of the inputs
    float avg = unit->bufmin * unit->avgFactor;
    unit->bufmin = 0;
    int c = unit->inc - 1;
    if( c > 0 ) memmove( unit->inputs, unit->inputs+1, sizeof(fann_type) * c );
    unit->inputs[c] = avg;

    //do the ANN

    fann_type *annOut = fann_rt_run( unit->ann, unit->inputs );

    int out_c = unit->outc;
    for( int i = 0; i < out_c; i++ )
    {
      float val = annOut[i];
      OUT0(i) = val;
      unit->out[i] = val;
    }
  }
  else {
    int out_c = unit->outc;
    for( int i = 0; i < out_c; i++ )
    {
      OUT0(i) = unit->out[i];
    }
  }
  unit->bufph = ph;
}

#if 0
static void Ann_next_k(Ann *unit, int numSamples)
{
  int in_c = unit->layer_v[0];
  int out_c = unit->layer_v[ unit->layer_c - 1 ];
//#if 0
  for( int c = 0; c < in_c; c++ )
  {
    int c_ = IN_INPUTS + c;
    unit->inputs[c] = IN0(c_);
  }

  fann_type *annOut = fann_run( unit->ann, unit->inputs );

  for( int c = 0; c < out_c; c++ )
  {
    float *out = OUT(c);
    float val = annOut[c];
    for( int s = 0; s < numSamples; ++s ) out[s] = val;
  }
//#endif
}

void Ann_next(Ann *unit, int numSamples)
{
  int in_c = unit->layer_v[0];
  int out_c = unit->layer_v[ unit->layer_c - 1 ];

  Ann_next_k( unit, 1 );

//#if 0
  for( int s = 1; s < numSamples; ++s ) {
    // copy data into input array
    for( int c = 0; c < in_c; c++ )
    {
      if( INRATE(c) != calc_FullRate ) continue;
      int c_ = IN_INPUTS + c;
      float *in = IN(c_);
      unit->inputs[c] = in[s];
    }

    // run the ANN
    fann_type *annOut = fann_run( unit->ann, unit->inputs );

    // write ANN output to buffer
    for( int c = 0; c < out_c; c++ )
    {
      float *out = OUT(c);
      out[s] = annOut[c];
    }
  }
//#endif
}
#endif