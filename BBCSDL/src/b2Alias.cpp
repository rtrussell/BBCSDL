// This module creates unmangled and unhidden aliases for functions and methods,
// also the Debug Draw interface via SDL2_gfx, for use by BBC BASIC for SDL 2.0.
// Version 2.0, (c) Richard T. Russell http://www.rtrussell.co.uk/, 06-Apr-2021

#include "Box2D.h"
#define VISIBLE __attribute__ ((visibility ("default")))

// Globals:
void *Renderer;
double xyScale, xOffset, yOffset;

// Debug draw functions:

class DebugDraw : public b2Draw { // b2Draw has all the virtual functions that we need to override here
public:
    // We won't be implementing all of these, but if we don't declare them here we'll get an override error
    void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color);
    void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color);
    void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color);
    void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color);
    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color);
    void DrawTransform(const b2Transform& xf);
    void DrawPoint(const b2Vec2&, float32, const b2Color&);
};

extern "C" {

extern int _ZN7b2WorldC1ERK6b2Vec2(void*, void*) ;
extern int _ZN13b2CircleShapeC1Ev(void*) ;
extern int _ZN14b2PolygonShapeC1Ev(void*) ;
extern int _ZN12b2ChainShapeC1Ev(void*) ;
extern void _ZN6b2Body11SetUserDataEPv(void*, void*) ;
extern void _ZN9b2Fixture11SetUserDataEPv(void*, void*) ;
extern void _ZN7b2Joint11SetUserDataEPv(void*, void*) ;
extern void _ZN7b2World11DestroyBodyEP6b2Body(void*, void*) ;
extern void _ZN12b2ChainShape13SetNextVertexERK6b2Vec2(void*, void*) ;
extern void _ZN12b2ChainShape13SetPrevVertexERK6b2Vec2(void*, void*) ;
extern void _ZN9b2Fixture13SetFilterDataERK8b2Filter(void*, void*) ;
extern void _ZN6b2Body14DestroyFixtureEP9b2Fixture(void*, void*) ;
extern void _ZN6b2Body17SetLinearVelocityERK6b2Vec2(void*, void*) ;
extern void _ZN7b2World10SetGravityERK6b2Vec2(void*, void*) ;
extern void _ZN12b2MouseJoint9SetTargetERK6b2Vec2(void*, void*) ;
extern void _ZN7b2World12DestroyJointEP7b2Joint(void*, void*) ;

#ifdef __EMSCRIPTEN__
extern void _ZNK15b2DistanceJoint10GetAnchorAEv(void*, void*) ;
extern void _ZNK15b2DistanceJoint10GetAnchorBEv(void*, void*) ;
extern void _ZNK13b2PulleyJoint10GetAnchorAEv(void*, void*) ;
extern void _ZNK13b2PulleyJoint10GetAnchorBEv(void*, void*) ;
extern void _ZNK11b2RopeJoint10GetAnchorAEv(void*, void*) ;
extern void _ZNK11b2RopeJoint10GetAnchorBEv(void*, void*) ;
#else
// On some platforms these return a b2Vec2, on others the b2Vec2 is passed as the first parameter:
extern long long _ZNK15b2DistanceJoint10GetAnchorAEv(void*, void*) ;
extern long long _ZNK15b2DistanceJoint10GetAnchorBEv(void*, void*) ;
extern long long _ZNK13b2PulleyJoint10GetAnchorAEv(void*, void*) ;
extern long long _ZNK13b2PulleyJoint10GetAnchorBEv(void*, void*) ;
extern long long _ZNK11b2RopeJoint10GetAnchorAEv(void*, void*) ;
extern long long _ZNK11b2RopeJoint10GetAnchorBEv(void*, void*) ;
#endif

extern bool _ZNK6b2Body7IsAwakeEv(void*) ;
extern bool _ZNK9b2Contact10IsTouchingEv(void*) ;
extern int _ZNK9b2Contact14GetChildIndexAEv(void*) ;
extern int _ZNK9b2Contact14GetChildIndexBEv(void*) ;
extern float _ZNK6b2Body7GetMassEv(void*) ;
extern float _ZNK6b2Body18GetAngularVelocityEv(void*) ;
extern void* _ZNK6b2Body17GetLinearVelocityEv(void*) ;

extern void* _ZN7b2World10CreateBodyEPK9b2BodyDef(void*, void*) ;
extern void* _ZN9b2Fixture7GetBodyEv(void*) ;
extern void* _ZN9b2Fixture8GetShapeEv(void*) ;
extern void* _ZNK6b2Body12GetTransformEv(void*) ;
extern void* _ZNK6b2Body11GetUserDataEv(void*) ;
extern void* _ZNK9b2Fixture11GetUserDataEv(void*) ;
extern void* _ZNK7b2Joint11GetUserDataEv(void*) ;
extern void* _ZN7b2Joint8GetBodyAEv(void*) ;
extern void* _ZN7b2Joint8GetBodyBEv(void*) ;
extern void* _ZN7b2World14GetContactListEv(void*) ;
extern void* _ZN6b2Body14GetContactListEv(void*) ;
extern void* _ZN9b2Contact7GetNextEv(void*) ;
extern void* _ZN9b2Contact11GetFixtureAEv(void*) ;
extern void* _ZN9b2Contact11GetFixtureBEv(void*) ;
extern void* _ZN7b2World11GetBodyListEv(void*) ;
extern void* _ZN6b2Body7GetNextEv(void*) ;
extern void* _ZN6b2Body13CreateFixtureEPK12b2FixtureDef(void*, void*) ;
extern void* _ZN7b2World11CreateJointEPK10b2JointDef(void*, void*) ;

extern void* _ZN6b2Body13CreateFixtureEPK7b2Shapef(void*, void*, float) ;

extern void _ZN6b2Body9SetActiveEb(void*, bool) ;
extern void _ZN6b2Body8SetAwakeEb(void*, bool) ;
extern void _ZN6b2Body9SetBulletEb(void*, bool) ;
extern void _ZN6b2Body16SetFixedRotationEb(void*, bool) ;
extern void _ZN9b2Fixture9SetSensorEb(void*, bool) ;
extern void _ZN6b2Body18SetSleepingAllowedEb(void*, bool) ;

extern void _ZN16b2PrismaticJoint11EnableMotorEb(void*, bool) ;
extern void _ZN15b2RevoluteJoint11EnableMotorEb(void*, bool) ;
extern void _ZN12b2WheelJoint11EnableMotorEb(void*, bool) ;

extern void _ZN12b2ChainShape11CreateChainEPK6b2Vec2i(void*, void*, int) ;
extern void _ZN12b2ChainShape10CreateLoopEPK6b2Vec2i(void*, void*, int) ;
extern void _ZN14b2PolygonShape3SetEPK6b2Vec2i(void*, void*, int) ;

extern void _ZN6b2Body12SetTransformERK6b2Vec2f(void*, void*, float) ;
extern void _ZN14b2PolygonShape8SetAsBoxEff(void*, float, float) ;
extern void _ZN14b2PolygonShape8SetAsBoxEffRK6b2Vec2f(void*, float, float, void*, float) ;
extern void _ZN7b2World4StepEfii(void*, float, int, int) ;

extern void _ZN6b2Body10ApplyForceERK6b2Vec2S2_b(void*, void*, void*, bool) ;
extern void _ZN6b2Body18ApplyLinearImpulseERK6b2Vec2S2_b(void*, void*, void*, bool) ;
extern void _ZN6b2Body11ApplyTorqueEfb(void*, float, bool) ;

extern void _ZN6b2Body18SetAngularVelocityEf(void*, float) ;
extern void _ZN16b2PrismaticJoint16SetMaxMotorForceEf(void*, float) ;
extern void _ZN15b2RevoluteJoint17SetMaxMotorTorqueEf(void*, float) ;
extern void _ZN12b2WheelJoint17SetMaxMotorTorqueEf(void*, float) ;
extern void _ZN16b2PrismaticJoint13SetMotorSpeedEf(void*, float) ;
extern void _ZN15b2RevoluteJoint13SetMotorSpeedEf(void*, float) ;
extern void _ZN12b2WheelJoint13SetMotorSpeedEf(void*, float) ;

extern void _ZN18b2DistanceJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_(void*, void*, void*, void*, void*) ;
extern void _ZN18b2FrictionJointDef10InitializeEP6b2BodyS1_RK6b2Vec2(void*, void*, void*, void*) ;
extern void _ZN19b2PrismaticJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_(void*, void*, void*, void*, void*) ;
extern void _ZN16b2PulleyJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_S4_S4_f(void*, void*, void*, void*, void*, void*, void*, float) ;
extern void _ZN18b2RevoluteJointDef10InitializeEP6b2BodyS1_RK6b2Vec2(void*, void*, void*, void*) ;
extern void _ZN14b2WeldJointDef10InitializeEP6b2BodyS1_RK6b2Vec2(void*, void*, void*, void*) ;
extern void _ZN15b2WheelJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_(void*, void*, void*, void*, void*) ;

extern void _ZN7b2World13DrawDebugDataEv(void*) ;

#ifndef __EMSCRIPTEN__
VISIBLE int b2NewWorld(void* a, void* b) { return _ZN7b2WorldC1ERK6b2Vec2(a, b); }
VISIBLE int b2CircleShape(void* a) { return _ZN13b2CircleShapeC1Ev(a); }
VISIBLE int b2PolygonShape(void* a) { return _ZN14b2PolygonShapeC1Ev(a); }
VISIBLE int b2ChainShape(void* a) { return _ZN12b2ChainShapeC1Ev(a); }
VISIBLE void b2SetUserDataB(void* a, void* b) { _ZN6b2Body11SetUserDataEPv(a, b); }
VISIBLE void b2SetUserDataF(void* a, void* b) { _ZN9b2Fixture11SetUserDataEPv(a, b); }
VISIBLE void b2SetUserDataJ(void* a, void* b) { _ZN7b2Joint11SetUserDataEPv(a, b); }
VISIBLE void b2DestroyBody(void* a, void* b) { _ZN7b2World11DestroyBodyEP6b2Body(a, b); }
VISIBLE void b2SetNextVertex(void* a, void* b) { _ZN12b2ChainShape13SetNextVertexERK6b2Vec2(a, b); }
VISIBLE void b2SetPrevVertex(void* a, void* b) { _ZN12b2ChainShape13SetPrevVertexERK6b2Vec2(a, b); }
VISIBLE void b2SetFilterData(void* a, void* b) { _ZN9b2Fixture13SetFilterDataERK8b2Filter(a, b); }
VISIBLE void b2DestroyFixture(void* a, void* b) { _ZN6b2Body14DestroyFixtureEP9b2Fixture(a, b); }
VISIBLE void b2SetLinearVelocity(void* a, void* b) { _ZN6b2Body17SetLinearVelocityERK6b2Vec2(a, b); }
VISIBLE void b2SetGravity(void* a, void* b) { _ZN7b2World10SetGravityERK6b2Vec2(a, b); }
VISIBLE void b2SetTarget(void* a, void* b) { _ZN12b2MouseJoint9SetTargetERK6b2Vec2(a, b); }
VISIBLE void b2DestroyJoint(void* a, void* b) { _ZN7b2World12DestroyJointEP7b2Joint(a, b); }

// On some platforms these return a b2Vec2, on others the b2Vec2 is passed as the first parameter:
VISIBLE long long b2DistanceJointGetAnchorA(void* a, void* b) { return _ZNK15b2DistanceJoint10GetAnchorAEv(a, b); }
VISIBLE long long b2DistanceJointGetAnchorB(void* a, void* b) { return _ZNK15b2DistanceJoint10GetAnchorBEv(a, b); }
VISIBLE long long b2PulleyJointGetAnchorA(void* a, void* b) { return _ZNK13b2PulleyJoint10GetAnchorAEv(a, b); }
VISIBLE long long b2PulleyJointGetAnchorB(void* a, void* b) { return _ZNK13b2PulleyJoint10GetAnchorBEv(a, b); }
VISIBLE long long b2RopeJointGetAnchorA(void* a, void* b) { return _ZNK11b2RopeJoint10GetAnchorAEv(a, b); }
VISIBLE long long b2RopeJointGetAnchorB(void* a, void* b) { return _ZNK11b2RopeJoint10GetAnchorBEv(a, b); }

VISIBLE bool b2IsAwake(void* a) { return _ZNK6b2Body7IsAwakeEv(a); }
VISIBLE bool b2IsTouching(void* a) { return _ZNK9b2Contact10IsTouchingEv(a); }
VISIBLE int b2GetChildIndexA(void* a) { return _ZNK9b2Contact14GetChildIndexAEv(a); }
VISIBLE int b2GetChildIndexB(void* a) { return _ZNK9b2Contact14GetChildIndexBEv(a); }
VISIBLE double b2GetMass(void* a) { return _ZNK6b2Body7GetMassEv(a); }
VISIBLE double b2GetAngularVelocity(void* a) { return _ZNK6b2Body18GetAngularVelocityEv(a); }
VISIBLE void* b2GetLinearVelocity(void* a) { return _ZNK6b2Body17GetLinearVelocityEv(a); }

VISIBLE void* b2CreateBody(void* a, void* b) { return _ZN7b2World10CreateBodyEPK9b2BodyDef(a, b); }
VISIBLE void* b2GetBody(void* a) { return _ZN9b2Fixture7GetBodyEv(a); }
VISIBLE void* b2GetShape(void* a) { return _ZN9b2Fixture8GetShapeEv(a); }
VISIBLE void* b2GetTransform(void* a) { return _ZNK6b2Body12GetTransformEv(a); }
VISIBLE void* b2GetUserDataB(void* a) { return _ZNK6b2Body11GetUserDataEv(a); }
VISIBLE void* b2GetUserDataF(void* a) { return _ZNK9b2Fixture11GetUserDataEv(a); }
VISIBLE void* b2GetUserDataJ(void* a) { return _ZNK7b2Joint11GetUserDataEv(a); }
VISIBLE void* b2GetBodyA(void* a) { return _ZN7b2Joint8GetBodyAEv(a); }
VISIBLE void* b2GetBodyB(void* a) { return _ZN7b2Joint8GetBodyBEv(a); }
VISIBLE void* b2GetContactListW(void* a) { return _ZN7b2World14GetContactListEv(a); }
VISIBLE void* b2GetContactListB(void* a) { return _ZN6b2Body14GetContactListEv(a); }
VISIBLE void* b2GetNextContact(void* a) { return _ZN9b2Contact7GetNextEv(a); }
VISIBLE void* b2GetFixtureA(void* a) { return _ZN9b2Contact11GetFixtureAEv(a); }
VISIBLE void* b2GetFixtureB(void* a) { return _ZN9b2Contact11GetFixtureBEv(a); }
VISIBLE void* b2GetBodyList(void* a) { return _ZN7b2World11GetBodyListEv(a); }
VISIBLE void* b2GetNextBody(void* a) { return _ZN6b2Body7GetNextEv(a); }
VISIBLE void* b2CreateFixtureFromDef(void* a, void* b) { return _ZN6b2Body13CreateFixtureEPK12b2FixtureDef(a, b); }
VISIBLE void* b2CreateJoint(void* a, void* b) { return _ZN7b2World11CreateJointEPK10b2JointDef(a, b); }

VISIBLE void* b2CreateFixtureFromShape(void* a, void* b, float c) { return _ZN6b2Body13CreateFixtureEPK7b2Shapef(a, b, c); }

VISIBLE void b2SetActive(void* a, bool b) { _ZN6b2Body9SetActiveEb(a, b); }
VISIBLE void b2SetAwake(void* a, bool b) { _ZN6b2Body8SetAwakeEb(a, b); }
VISIBLE void b2SetBullet(void* a, bool b) { _ZN6b2Body9SetBulletEb(a, b); }
VISIBLE void b2SetFixedRotation(void* a, bool b) { _ZN6b2Body16SetFixedRotationEb(a, b); }
VISIBLE void b2SetSensor(void* a, bool b) { _ZN9b2Fixture9SetSensorEb(a, b); }
VISIBLE void b2SetSleepingAllowed(void* a, bool b) { _ZN6b2Body18SetSleepingAllowedEb(a, b); }

VISIBLE void b2EnableMotorP(void* a, bool b) { _ZN16b2PrismaticJoint11EnableMotorEb(a, b); }
VISIBLE void b2EnableMotorR(void* a, bool b) { _ZN15b2RevoluteJoint11EnableMotorEb(a, b); }
VISIBLE void b2EnableMotorW(void* a, bool b) { _ZN12b2WheelJoint11EnableMotorEb(a, b); }

VISIBLE void b2CreateChain(void* a, void* b, int c) { _ZN12b2ChainShape11CreateChainEPK6b2Vec2i(a, b, c); }
VISIBLE void b2CreateLoop(void* a, void* b, int c) { _ZN12b2ChainShape10CreateLoopEPK6b2Vec2i(a, b, c); }
VISIBLE void b2SetPolygon(void* a, void* b, int c) { _ZN14b2PolygonShape3SetEPK6b2Vec2i(a, b, c); }

VISIBLE void b2SetTransform(void* a, void* b, float c) { _ZN6b2Body12SetTransformERK6b2Vec2f(a, b, c); }
VISIBLE void b2SetAsBox(void* a, float b, float c) { _ZN14b2PolygonShape8SetAsBoxEff(a, b, c); }
VISIBLE void b2SetAsOrientedBox(void* a, float b, float c, void* d, float e) { _ZN14b2PolygonShape8SetAsBoxEffRK6b2Vec2f(a, b, c, d, e); }
VISIBLE void b2WorldStep(void* a, float b, int c, int d) { _ZN7b2World4StepEfii(a, b, c, d); }

VISIBLE void b2ApplyForce(void* a, void* b, void* c, bool d) { _ZN6b2Body10ApplyForceERK6b2Vec2S2_b(a, b, c, d); }
VISIBLE void b2ApplyLinearImpulse(void* a, void* b, void* c, bool d) { _ZN6b2Body18ApplyLinearImpulseERK6b2Vec2S2_b(a, b, c, d); }
VISIBLE void b2ApplyTorque(void* a, float b, bool c) { _ZN6b2Body11ApplyTorqueEfb(a, b, c); }

VISIBLE void b2SetAngularVelocity(void* a, float b) { _ZN6b2Body18SetAngularVelocityEf(a, b); }
VISIBLE void b2SetMaxMotorForceP(void* a, float b) { _ZN16b2PrismaticJoint16SetMaxMotorForceEf(a, b); }
VISIBLE void b2SetMaxMotorTorqueR(void* a, float b) { _ZN15b2RevoluteJoint17SetMaxMotorTorqueEf(a, b); }
VISIBLE void b2SetMaxMotorTorqueW(void* a, float b) { _ZN12b2WheelJoint17SetMaxMotorTorqueEf(a, b); }
VISIBLE void b2SetMotorSpeedP(void* a, float b) { _ZN16b2PrismaticJoint13SetMotorSpeedEf(a, b); }
VISIBLE void b2SetMotorSpeedR(void* a, float b) { _ZN15b2RevoluteJoint13SetMotorSpeedEf(a, b); }
VISIBLE void b2SetMotorSpeedW(void* a, float b) { _ZN12b2WheelJoint13SetMotorSpeedEf(a, b); }

VISIBLE void b2DistanceJointInit(void* a, void* b, void* c, void* d, void* e) { _ZN18b2DistanceJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_(a, b, c, d, e); }
VISIBLE void b2FrictionJointInit(void* a, void* b, void* c, void* d) { _ZN18b2FrictionJointDef10InitializeEP6b2BodyS1_RK6b2Vec2(a, b, c, d); }
VISIBLE void b2PrismaticJointInit(void* a, void* b, void* c, void* d, void* e) { _ZN19b2PrismaticJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_(a, b, c, d, e); }
VISIBLE void b2PulleyJointInit(void* a, void* b, void* c, void* d, void* e, void* f, void* g, float h) { _ZN16b2PulleyJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_S4_S4_f(a, b, c, d, e, f, g, h); }
VISIBLE void b2RevoluteJointInit(void* a, void* b, void* c, void* d) { _ZN18b2RevoluteJointDef10InitializeEP6b2BodyS1_RK6b2Vec2(a, b, c, d); }
VISIBLE void b2WeldJointInit(void* a, void* b, void* c, void* d) { _ZN14b2WeldJointDef10InitializeEP6b2BodyS1_RK6b2Vec2(a, b, c, d); }
VISIBLE void b2WheelJointInit(void* a, void* b, void* c, void* d, void* e) { _ZN15b2WheelJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_(a, b, c, d, e); }

static int (*aaFilledPolygonColor)(void*, const double*, const double*, int, uint32);

VISIBLE void b2DrawDebugData(void* a) { _ZN7b2World13DrawDebugDataEv(a) ; }
VISIBLE void b2DebugInit(b2World* world, void* device, void* polygon, int flags)
    {
	Renderer = device;
	aaFilledPolygonColor = (int (*)(void*, const double*, const double*, int, uint32)) polygon;
	static DebugDraw debugdraw ;
	debugdraw.SetFlags(flags) ;
	world->SetDebugDraw(&debugdraw) ;
    }

VISIBLE void b2DebugMatrix(double scale, double xoff, double yoff)
    {
	xyScale = scale ;
	xOffset = xoff ;
	yOffset = yoff ;
    }

#endif

#ifdef __EMSCRIPTEN__
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
typedef size_t st ;
typedef double db ;

long long b2NewWorld(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZN7b2WorldC1ERK6b2Vec2((void*) a, (void*) b); }

long long b2CircleShape(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZN13b2CircleShapeC1Ev((void*) a); }

long long b2PolygonShape(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZN14b2PolygonShapeC1Ev((void*) a); }

long long b2ChainShape(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZN12b2ChainShapeC1Ev((void*) a); }

long long b2SetUserDataB(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body11SetUserDataEPv((void*) a, (void*) b); return 0; }

long long b2SetUserDataF(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN9b2Fixture11SetUserDataEPv((void*) a, (void*) b); return 0; }

long long b2SetUserDataJ(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN7b2Joint11SetUserDataEPv((void*) a, (void*) b); return 0; }

long long b2DestroyBody(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN7b2World11DestroyBodyEP6b2Body((void*) a, (void*) b); return 0; }

long long b2SetNextVertex(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN12b2ChainShape13SetNextVertexERK6b2Vec2((void*) a, (void*) b); return 0; }

long long b2SetPrevVertex(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN12b2ChainShape13SetPrevVertexERK6b2Vec2((void*) a, (void*) b); return 0; }

long long b2SetFilterData(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN9b2Fixture13SetFilterDataERK8b2Filter((void*) a, (void*) b); return 0; }

long long b2DestroyFixture(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body14DestroyFixtureEP9b2Fixture((void*) a, (void*) b); return 0; }

long long b2SetLinearVelocity(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body17SetLinearVelocityERK6b2Vec2((void*) a, (void*) b); return 0; }

long long b2SetGravity(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN7b2World10SetGravityERK6b2Vec2((void*) a, (void*) b); return 0; }

long long b2SetTarget(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN12b2MouseJoint9SetTargetERK6b2Vec2((void*) a, (void*) b); return 0; }

long long b2DestroyJoint(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN7b2World12DestroyJointEP7b2Joint((void*) a, (void*) b); return 0; }

long long b2DistanceJointGetAnchorA(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZNK15b2DistanceJoint10GetAnchorAEv((void*) a, (void*) b); return 0; }

long long b2DistanceJointGetAnchorB(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZNK15b2DistanceJoint10GetAnchorBEv((void*) a, (void*) b); return 0; }

long long b2PulleyJointGetAnchorA(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZNK13b2PulleyJoint10GetAnchorAEv((void*) a, (void*) b); return 0; }

long long b2PulleyJointGetAnchorB(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZNK13b2PulleyJoint10GetAnchorBEv((void*) a, (void*) b); return 0; }

long long b2RopeJointGetAnchorA(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZNK11b2RopeJoint10GetAnchorAEv((void*) a, (void*) b); return 0; }

long long b2RopeJointGetAnchorB(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZNK11b2RopeJoint10GetAnchorBEv((void*) a, (void*) b); return 0; }

long long b2IsAwake(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZNK6b2Body7IsAwakeEv((void*) a); }

long long b2IsTouching(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZNK9b2Contact10IsTouchingEv((void*) a); }

long long b2GetChildIndexA(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZNK9b2Contact14GetChildIndexAEv((void*) a); }

long long b2GetChildIndexB(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZNK9b2Contact14GetChildIndexBEv((void*) a); }

double b2GetMass(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZNK6b2Body7GetMassEv((void*) a); }

double b2GetAngularVelocity(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return _ZNK6b2Body18GetAngularVelocityEv((void*) a); }

long long b2GetLinearVelocity(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZNK6b2Body17GetLinearVelocityEv((void*) a); }

long long b2CreateBody(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN7b2World10CreateBodyEPK9b2BodyDef((void*) a, (void*) b); }

long long b2GetBody(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN9b2Fixture7GetBodyEv((void*) a); }

long long b2GetShape(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN9b2Fixture8GetShapeEv((void*) a); }

long long b2GetTransform(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZNK6b2Body12GetTransformEv((void*) a); }

long long b2GetUserDataB(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZNK6b2Body11GetUserDataEv((void*) a); }

long long b2GetUserDataF(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZNK9b2Fixture11GetUserDataEv((void*) a); }

long long b2GetUserDataJ(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZNK7b2Joint11GetUserDataEv((void*) a); }

long long b2GetBodyA(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN7b2Joint8GetBodyAEv((void*) a); }

long long b2GetBodyB(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN7b2Joint8GetBodyBEv((void*) a); }

long long b2GetContactListW(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN7b2World14GetContactListEv((void*) a); }

long long b2GetContactListB(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN6b2Body14GetContactListEv((void*) a); }

long long b2GetNextContact(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN9b2Contact7GetNextEv((void*) a); }

long long b2GetFixtureA(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN9b2Contact11GetFixtureAEv((void*) a); }

long long b2GetFixtureB(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN9b2Contact11GetFixtureBEv((void*) a); }

long long b2GetBodyList(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN7b2World11GetBodyListEv((void*) a); }

long long b2GetNextBody(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN6b2Body7GetNextEv((void*) a); }

long long b2CreateFixtureFromDef(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN6b2Body13CreateFixtureEPK12b2FixtureDef((void*) a, (void*) b); }

long long b2CreateJoint(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN7b2World11CreateJointEPK10b2JointDef((void*) a, (void*) b); }

long long b2CreateFixtureFromShape(st a, st b, st c, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) _ZN6b2Body13CreateFixtureEPK7b2Shapef((void*) a, (void*) b, *(float*)&c); }

long long b2SetActive(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body9SetActiveEb((void*) a, b); return 0; }

long long b2SetAwake(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body8SetAwakeEb((void*) a, b); return 0; }

long long b2SetBullet(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body9SetBulletEb((void*) a, b); return 0; }

long long b2SetFixedRotation(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body16SetFixedRotationEb((void*) a, b); return 0; }

long long b2SetSensor(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN9b2Fixture9SetSensorEb((void*) a, b); return 0; }

long long b2SetSleepingAllowed(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body18SetSleepingAllowedEb((void*) a, b); return 0; }

long long b2EnableMotorP(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN16b2PrismaticJoint11EnableMotorEb((void*) a, b); return 0; }

long long b2EnableMotorR(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN15b2RevoluteJoint11EnableMotorEb((void*) a, b); return 0; }

long long b2EnableMotorW(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN12b2WheelJoint11EnableMotorEb((void*) a, b); return 0; }

long long b2CreateChain(st a, st b, st c, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN12b2ChainShape11CreateChainEPK6b2Vec2i((void*) a, (void*) b, c); return 0; }

long long b2CreateLoop(st a, st b, st c, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN12b2ChainShape10CreateLoopEPK6b2Vec2i((void*) a, (void*) b, c); return 0; }

long long b2SetPolygon(st a, st b, st c, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN14b2PolygonShape3SetEPK6b2Vec2i((void*) a, (void*) b, c); return 0; }

long long b2SetTransform(st a, st b, st c, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body12SetTransformERK6b2Vec2f((void*) a, (void*) b, *(float*)&c); return 0; }

long long b2SetAsBox(st a, st b, st c, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN14b2PolygonShape8SetAsBoxEff((void*) a, *(float*)&b, *(float*)&c); return 0; }

long long b2SetAsOrientedBox(st a, st b, st c, st d, st e, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN14b2PolygonShape8SetAsBoxEffRK6b2Vec2f((void*) a, *(float*)&b, *(float*)&c, (void*) d, *(float*)&e); return 0; }

long long b2WorldStep(st a, st b, st c, st d, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN7b2World4StepEfii((void*) a, *(float*)&b, c, d); return 0; }

long long b2ApplyForce(st a, st b, st c, st d, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body10ApplyForceERK6b2Vec2S2_b((void*) a, (void*) b, (void*) c, d); return 0; }

long long b2ApplyLinearImpulse(st a, st b, st c, st d, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body18ApplyLinearImpulseERK6b2Vec2S2_b((void*) a, (void*) b, (void*) c, d); return 0; }

long long b2ApplyTorque(st a, st b, st c, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body11ApplyTorqueEfb((void*) a, *(float*)&b, c); return 0; }

long long b2SetAngularVelocity(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN6b2Body18SetAngularVelocityEf((void*) a, *(float*)&b); return 0; }

long long b2SetMaxMotorForceP(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN16b2PrismaticJoint16SetMaxMotorForceEf((void*) a, *(float*)&b); return 0; }

long long b2SetMaxMotorTorqueR(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN15b2RevoluteJoint17SetMaxMotorTorqueEf((void*) a, *(float*)&b); return 0; }

long long b2SetMaxMotorTorqueW(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN12b2WheelJoint17SetMaxMotorTorqueEf((void*) a, *(float*)&b); return 0; }

long long b2SetMotorSpeedP(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN16b2PrismaticJoint13SetMotorSpeedEf((void*) a, *(float*)&b); return 0; }

long long b2SetMotorSpeedR(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN15b2RevoluteJoint13SetMotorSpeedEf((void*) a, *(float*)&b); return 0; }

long long b2SetMotorSpeedW(st a, st b, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN12b2WheelJoint13SetMotorSpeedEf((void*) a, *(float*)&b); return 0; }

long long b2DistanceJointInit(st a, st b, st c, st d, st e, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN18b2DistanceJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_((void*) a, (void*) b, (void*) c, (void*) d, (void*) e); return 0; }

long long b2FrictionJointInit(st a, st b, st c, st d, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN18b2FrictionJointDef10InitializeEP6b2BodyS1_RK6b2Vec2((void*) a, (void*) b, (void*) c, (void*) d); return 0; }

long long b2PrismaticJointInit(st a, st b, st c, st d, st e, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN19b2PrismaticJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_((void*) a, (void*) b, (void*) c, (void*) d, (void*) e); return 0; }

long long b2PulleyJointInit(st a, st b, st c, st d, st e, st f, st g, st h,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN16b2PulleyJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_S4_S4_f((void*) a, (void*) b, (void*) c, (void*) d, (void*) e, (void*) f, (void*) g, *(float*)&h); return 0; }

long long b2RevoluteJointInit(st a, st b, st c, st d, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN18b2RevoluteJointDef10InitializeEP6b2BodyS1_RK6b2Vec2((void*) a, (void*) b, (void*) c, (void*) d); return 0; }

long long b2WeldJointInit(st a, st b, st c, st d, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN14b2WeldJointDef10InitializeEP6b2BodyS1_RK6b2Vec2((void*) a, (void*) b, (void*) c, (void*) d); return 0; }

long long b2WheelJointInit(st a, st b, st c, st d, st e, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN15b2WheelJointDef10InitializeEP6b2BodyS1_RK6b2Vec2S4_((void*) a, (void*) b, (void*) c, (void*) d, (void*) e); return 0; }

long long b2DrawDebugData(st a, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ _ZN7b2World13DrawDebugDataEv((void*) a); return 0; }

long long b2DebugInit(st a, st b, st c, st d, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
    {
	b2World *world = (b2World*) a ;
	Renderer = (void*) b ;
	static DebugDraw debugdraw;
	debugdraw.SetFlags(d) ;
	world->SetDebugDraw(&debugdraw);
	return 0 ;
    }

long long b2DebugMatrix(st i0, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
    {
	xyScale = f0 ;
	xOffset = f1 ;
	yOffset = f2 ;
	return 0 ;
    }

#define B2NSYS 82
#define B2POW2 128 // smallest power-of-2 >= B2NSYS

static const char *B2name[B2NSYS] = {
	"b2ApplyForce",
	"b2ApplyLinearImpulse",
	"b2ApplyTorque",
	"b2ChainShape",
	"b2CircleShape",
	"b2CreateBody",
	"b2CreateChain",
	"b2CreateFixtureFromDef",
	"b2CreateFixtureFromShape",
	"b2CreateJoint",
	"b2CreateLoop",
	"b2DebugInit",
	"b2DebugMatrix",
	"b2DestroyBody",
	"b2DestroyFixture",
	"b2DestroyJoint",
	"b2DistanceJointGetAnchorA",
	"b2DistanceJointGetAnchorB",
	"b2DistanceJointInit",
	"b2DrawDebugData",
	"b2EnableMotorP",
	"b2EnableMotorR",
	"b2EnableMotorW",
	"b2FrictionJointInit",
	"b2GetBody",
	"b2GetBodyA",
	"b2GetBodyB",
	"b2GetBodyList",
	"b2GetChildIndexA",
	"b2GetChildIndexB",
	"b2GetContactListB",
	"b2GetContactListW",
	"b2GetFixtureA",
	"b2GetFixtureB",
	"b2GetLinearVelocity",
	"b2GetNextBody",
	"b2GetNextContact",
	"b2GetShape",
	"b2GetTransform",
	"b2GetUserDataB",
	"b2GetUserDataF",
	"b2GetUserDataJ",
	"b2IsAwake",
	"b2IsTouching",
	"b2NewWorld",
	"b2PolygonShape",
	"b2PrismaticJointInit",
	"b2PulleyJointGetAnchorA",
	"b2PulleyJointGetAnchorB",
	"b2PulleyJointInit",
	"b2RevoluteJointInit",
	"b2RopeJointGetAnchorA",
	"b2RopeJointGetAnchorB",
	"b2SetActive",
	"b2SetAngularVelocity",
	"b2SetAsBox",
	"b2SetAsOrientedBox",
	"b2SetAwake",
	"b2SetBullet",
	"b2SetFilterData",
	"b2SetFixedRotation",
	"b2SetGravity",
	"b2SetLinearVelocity",
	"b2SetMaxMotorForceP",
	"b2SetMaxMotorTorqueR",
	"b2SetMaxMotorTorqueW",
	"b2SetMotorSpeedP",
	"b2SetMotorSpeedR",
	"b2SetMotorSpeedW",
	"b2SetNextVertex",
	"b2SetPolygon",
	"b2SetPrevVertex",
	"b2SetSensor",
	"b2SetSleepingAllowed",
	"b2SetTarget",
	"b2SetTransform",
	"b2SetUserDataB",
	"b2SetUserDataF",
	"b2SetUserDataJ",
	"b2WeldJointInit",
	"b2WheelJointInit",
	"b2WorldStep"} ;

static long long (*B2func[B2NSYS])(st, st, st, st, st, st, st, st, st, st, st, st,
				db, db, db, db, db, db, db, db) = {
	b2ApplyForce,
	b2ApplyLinearImpulse,
	b2ApplyTorque,
	b2ChainShape,
	b2CircleShape,
	b2CreateBody,
	b2CreateChain,
	b2CreateFixtureFromDef,
	b2CreateFixtureFromShape,
	b2CreateJoint,
	b2CreateLoop,
	b2DebugInit,
	b2DebugMatrix,
	b2DestroyBody,
	b2DestroyFixture,
	b2DestroyJoint,
	b2DistanceJointGetAnchorA,
	b2DistanceJointGetAnchorB,
	b2DistanceJointInit,
	b2DrawDebugData,
	b2EnableMotorP,
	b2EnableMotorR,
	b2EnableMotorW,
	b2FrictionJointInit,
	b2GetBody,
	b2GetBodyA,
	b2GetBodyB,
	b2GetBodyList,
	b2GetChildIndexA,
	b2GetChildIndexB,
	b2GetContactListB,
	b2GetContactListW,
	b2GetFixtureA,
	b2GetFixtureB,
	b2GetLinearVelocity,
	b2GetNextBody,
	b2GetNextContact,
	b2GetShape,
	b2GetTransform,
	b2GetUserDataB,
	b2GetUserDataF,
	b2GetUserDataJ,
	b2IsAwake,
	b2IsTouching,
	b2NewWorld,
	b2PolygonShape,
	b2PrismaticJointInit,
	b2PulleyJointGetAnchorA,
	b2PulleyJointGetAnchorB,
	b2PulleyJointInit,
	b2RevoluteJointInit,
	b2RopeJointGetAnchorA,
	b2RopeJointGetAnchorB,
	b2SetActive,
	b2SetAngularVelocity,
	b2SetAsBox,
	b2SetAsOrientedBox,
	b2SetAwake,
	b2SetBullet,
	b2SetFilterData,
	b2SetFixedRotation,
	b2SetGravity,
	b2SetLinearVelocity,
	b2SetMaxMotorForceP,
	b2SetMaxMotorTorqueR,
	b2SetMaxMotorTorqueW,
	b2SetMotorSpeedP,
	b2SetMotorSpeedR,
	b2SetMotorSpeedW,
	b2SetNextVertex,
	b2SetPolygon,
	b2SetPrevVertex,
	b2SetSensor,
	b2SetSleepingAllowed,
	b2SetTarget,
	b2SetTransform,
	b2SetUserDataB,
	b2SetUserDataF,
	b2SetUserDataJ,
	b2WeldJointInit,
	b2WheelJointInit,
	b2WorldStep} ;

long long B2D_GetProcAddress(st symbol, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
    {
	int b = 0, h = B2POW2, r = 0 ;
	if (strcmp((const char *)symbol, "b2GetMass") == 0) return (intptr_t) b2GetMass ;
	if (strcmp((const char *)symbol, "b2GetAngularVelocity") == 0) return (intptr_t) b2GetAngularVelocity ;
	do
	    {
		h /= 2 ;
		if (((b + h) < B2NSYS) && ((r = strcmp ((const char*) symbol, B2name[b + h])) >= 0))
			b += h ;
	    }
	while (h) ;
	if (r == 0) return (intptr_t) B2func[b] ;
	return 0 ;
    }

int aaFilledPolygonColor(void*, const double*, const double*, int, uint32);
#endif 
}

static uint32 abgr(const b2Color& color)
{
	uint8 r = color.r * 255;
	uint8 g = color.g * 255;
	uint8 b = color.b * 255;
	uint8 a = color.a * 224;
	return (a << 24) | (b << 16) | (g << 8) | r;
}

void DebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
	double xv[4], yv[4] ;
	double x1 = xOffset + xyScale * p1.x, y1 = yOffset - xyScale * p1.y ;
	double x2 = xOffset + xyScale * p2.x, y2 = yOffset - xyScale * p2.y ;
	double dx = x2 - x1, dy = y2 - y1 ;
	double d = 0.75 / sqrt(dx*dx + dy*dy) ; // line thickness 1.5 pixels
	dx *= d ; dy *= d ;
	xv[0] = x1 + dy ; yv[0] = y1 - dx ;
	xv[1] = x1 - dy ; yv[1] = y1 + dx ;
	xv[2] = x2 - dy ; yv[2] = y2 + dx ;
	xv[3] = x2 + dy ; yv[3] = y2 - dx ;
	aaFilledPolygonColor(Renderer, xv, yv, 4, abgr(color));
}

void DebugDraw::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
	double *xv = (double *)malloc(vertexCount * sizeof(double));
	double *yv = (double *)malloc(vertexCount * sizeof(double));
	for (int i = 0; i < vertexCount; i++)
	    {
		xv[i] = xOffset + vertices[i].x * xyScale ;
		yv[i] = yOffset - vertices[i].y * xyScale ;
	    }
	aaFilledPolygonColor(Renderer, xv, yv, vertexCount, abgr(color));
	free(xv) ;
	free(yv) ;
}

void DebugDraw::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
{
	b2Color grey = {0.5, 0.5, 0.5, 1.0} ;
	b2Vec2 spoke = {center.x + axis.x * radius, center.y + axis.y * radius} ;
	int nverts = xyScale * radius + 3.0 ;
	if (nverts > 90) nverts = 90 ;
	double *xv = (double *)malloc(nverts * sizeof(double));
	double *yv = (double *)malloc(nverts * sizeof(double));
	for (int i = 0; i < nverts; i++)
	    {
		double angle = 2.0 * 3.141592654 * (double) i / (double) nverts ; 
		xv[i] = xOffset + xyScale * (center.x + radius * cos(angle));
		yv[i] = yOffset - xyScale * (center.y + radius * sin(angle));
	    }
	aaFilledPolygonColor(Renderer, xv, yv, nverts, abgr(color));
	DebugDraw::DrawSegment(center, spoke, grey) ;
	free(xv) ;
	free(yv) ;
}

void DebugDraw::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
	for (int i = 0; i < vertexCount; i++)
		DebugDraw::DrawSegment(vertices[i], vertices[(i + 1) % vertexCount], color) ;
}

void DebugDraw::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) {}
void DebugDraw::DrawTransform(const b2Transform& xf) {}
void DebugDraw::DrawPoint(const b2Vec2&, float32, const b2Color&) {}

