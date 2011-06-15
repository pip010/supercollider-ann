#ifndef ANN_POOL_H
#define ANN_POOL_H

#include <floatfann.h>
#include <string.h>

struct AnnDef {
  AnnDef( const char *name, struct fann *ann ) : _name(strdup(name)), _ann(ann) {}
  ~AnnDef() { free(_name); }
  char *_name;
  struct fann *_ann;
};

extern const int kAnnDefCount;
extern AnnDef * annDefs[];

void initPool();

#endif // ANN_POOL_H