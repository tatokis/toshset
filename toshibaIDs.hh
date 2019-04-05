
#ifndef __toshibaIDs_h__
#define __toshibaIDs_h__

/*
 * structure with model-specific information
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int id;
  const char* name;
} ToshibaID;

extern int         checkToshibaID(int id);
extern const char* toshibaModelName(int id);
#ifdef __cplusplus
}
#endif

#endif /* __toshibaIDs_h__ */
