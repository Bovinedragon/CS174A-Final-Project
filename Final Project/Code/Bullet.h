#ifndef __BULLET_H__
#define __BULLET_H__

#include "Object.h"

class Bullet : public Object
{
public:
	Bullet(vec3 position) : Object(position){};
};


#endif