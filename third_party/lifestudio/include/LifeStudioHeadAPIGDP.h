#ifndef _LIFESTUDIOHEADAPIGDP_H_
#define _LIFESTUDIOHEADAPIGDP_H_
// ============================================================================
//  LifeStudio:Head API - interoperability declarations (GDP geometry file).
// ============================================================================

#include "LifeStudioHeadAPITransform.h"

namespace LifeStudioHeadAPI
{

#define LIFESTUDIOHEADAPI_MATERIAL_WRAPU       0x0001
#define LIFESTUDIOHEADAPI_MATERIAL_WRAPV       0x0002
#define LIFESTUDIOHEADAPI_MATERIAL_UVCHG       0x0004
#define LIFESTUDIOHEADAPI_MATERIAL_BLEND       0x0008
#define LIFESTUDIOHEADAPI_MATERIAL_TRANPARENT  0x0010
#define LIFESTUDIOHEADAPI_MATERIAL_DOUBLESIDED 0x0020

struct ObjectMaterial
{
  char         name[32];
  char         _unused1[16];
  float        ambient[4]; 
  char         _unused2[48];
  unsigned int flags;
  char         textureName[128];
};

struct IGDPObject : public ITransformerInput
{
  virtual int LIFESTUDIOHEADAPICALL MaterialsCount() const = 0;
  virtual bool LIFESTUDIOHEADAPICALL Material(int materialNumber, ObjectMaterial &material) const = 0;
  virtual int LIFESTUDIOHEADAPICALL TrianglesCount(int materialNumber) const = 0;
  virtual const unsigned short *LIFESTUDIOHEADAPICALL Triangulation(int materialNumber) const = 0;
  virtual int LIFESTUDIOHEADAPICALL VerticesCount() const = 0;
  virtual const float *LIFESTUDIOHEADAPICALL UV() const = 0;
  virtual const float *LIFESTUDIOHEADAPICALL UVNoChg() const = 0;
  virtual bool LIFESTUDIOHEADAPICALL HasExtenedUVInfo() const = 0;
  virtual int LIFESTUDIOHEADAPICALL UVCount() const = 0;
  virtual int LIFESTUDIOHEADAPICALL BaseTrianglesCount() const = 0;
  virtual const unsigned short *LIFESTUDIOHEADAPICALL BaseTriangulation() const = 0;
  virtual const unsigned short *LIFESTUDIOHEADAPICALL UV2VMap() const = 0;
  virtual int LIFESTUDIOHEADAPICALL AdditionalNormalsDataSize() const = 0;
  virtual bool LIFESTUDIOHEADAPICALL AdditionalNormalsData(char *buffer) = 0;
  virtual int LIFESTUDIOHEADAPICALL PNGTextureSize(const char *textureName) const = 0;
  virtual bool LIFESTUDIOHEADAPICALL PNGTexture(const char *textureName, char *buffer) = 0;
  virtual bool LIFESTUDIOHEADAPICALL IsTransformable() const = 0;
  virtual int LIFESTUDIOHEADAPICALL DataListSize() const = 0;
  virtual const char *LIFESTUDIOHEADAPICALL DataListItem(int itemNumber) const = 0;
  virtual int LIFESTUDIOHEADAPICALL DefaultAnimatorDataSize() const = 0;
  virtual bool LIFESTUDIOHEADAPICALL DefaultAnimatorData(char *buffer) = 0;
  virtual int LIFESTUDIOHEADAPICALL SubObjectsCount() const = 0;
  virtual const char *LIFESTUDIOHEADAPICALL SubObjectName(int number) const = 0;
  virtual const char *LIFESTUDIOHEADAPICALL SubObjectType(int number) const = 0;
  virtual IGDPObject *LIFESTUDIOHEADAPICALL SubObject(int number) = 0;
  virtual void LIFESTUDIOHEADAPICALL Destroy() = 0;
};

struct IGDPFile
{
  virtual int LIFESTUDIOHEADAPICALL ObjectsCount() const = 0;
  virtual const char *LIFESTUDIOHEADAPICALL ObjectName(int number) const = 0;
  virtual IGDPObject *LIFESTUDIOHEADAPICALL Object(int number) = 0;
  virtual void LIFESTUDIOHEADAPICALL Destroy() = 0;

  static LIFESTUDIOHEADAPI_API IGDPFile *LIFESTUDIOHEADAPICALL Create(const char *filename);
};

};

#endif
