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
  float        diffuse[4];
  float        ambient[4];
  float        specular[4];
  float        emission[4];
  float        shininess;
  unsigned int flags;
  char         textureName[32];
};

struct IGDPObject : public ITransformerInput
{
  virtual int MaterialsCount() const = 0;
  virtual bool Material(int materialNumber, ObjectMaterial &material) const = 0;
  virtual int TrianglesCount(int materialNumber) const = 0;
  virtual const unsigned short *Triangulation(int materialNumber) const = 0;
  virtual int VerticesCount() const = 0;
  virtual const float *UV() const = 0;
  virtual const float *UVNoChg() const = 0;
  virtual bool HasExtenedUVInfo() const = 0;
  virtual int UVCount() const = 0;
  virtual int BaseTrianglesCount() const = 0;
  virtual const unsigned short *BaseTriangulation() const = 0;
  virtual const unsigned short *UV2VMap() const = 0;
  virtual int AdditionalNormalsDataSize() const = 0;
  virtual bool AdditionalNormalsData(char *buffer) = 0;
  virtual int PNGTextureSize(const char *textureName) const = 0;
  virtual bool PNGTexture(const char *textureName, char *buffer) = 0;
  virtual bool IsTransformable() const = 0;
  virtual int DataListSize() const = 0;
  virtual const char *DataListItem(int itemNumber) const = 0;
  virtual int DefaultAnimatorDataSize() const = 0;
  virtual bool DefaultAnimatorData(char *buffer) = 0;
  virtual int SubObjectsCount() const = 0;
  virtual const char *SubObjectName(int number) const = 0;
  virtual const char *SubObjectType(int number) const = 0;
  virtual IGDPObject *SubObject(int number) = 0;
  virtual void Destroy() = 0;
};

struct IGDPFile
{
  virtual int ObjectsCount() const = 0;
  virtual const char *ObjectName(int number) const = 0;
  virtual IGDPObject *Object(int number) = 0;
  virtual void Destroy() = 0;

  static LIFESTUDIOHEADAPI_API IGDPFile *__stdcall Create(const char *filename);
};

};

#endif
