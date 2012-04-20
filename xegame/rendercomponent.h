#ifndef RENDER_COMPONENT_INCLUDED
#define RENDER_COMPONENT_INCLUDED

#include "component.h"

class Engine;
class ModelResource;
class Model;

class RenderComponent : public Component
{
public:
	RenderComponent( Engine* engine, const char* asset );	// spacetree probably  sufficient, but 'engine' easier to keep track of
	virtual ~RenderComponent();

	virtual RenderComponent*	ToRender()		{ return this; }

	virtual void OnAdd( Chit* chit );
	virtual void OnRemove();

private:
	Engine* engine;
	const ModelResource* resource;
	Model* model;
};

#endif // RENDER_COMPONENT_INCLUDED