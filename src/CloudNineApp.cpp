#ifdef _DEBUG
#pragma comment(lib, "DSAPI.dbg.lib")
#else
#pragma comment(lib, "DSAPI.lib")
#endif
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/Camera.h"
#include "cinder/GeomIo.h"
#include "cinder/MayaCamUI.h"
#include "cinder/params/Params.h"
#include "cinder/Rand.h"
#include "cinder/Perlin.h"
#include "CiDSAPI.h"

using namespace ci;
using namespace ci::app;
using namespace ci::params;
using namespace std;
using namespace CinderDS;

static float S_MIN_Z = 100.0f;
static float S_MAX_Z = 1000.0f;

class CloudNineApp : public App
{
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void update() override;
	void draw() override;
	void cleanup() override;

private:
	void setupDS();
	void setupScene();
	void setupPointCloud();
	void setupParticles();
	void setupFBO();
	void setupGUI();

	void updatePointCloud();
	void updateParticles();
	void updateFBO();

	void drawPointCloud();
	void drawParticles();
	void drawSkyBox();
	void drawGUI();

	//DSAPI
	CinderDSRef			mCinderDS;
	gl::Texture2dRef	mTexRgb;
	Channel16u			mChanDepth;
	ivec2				mDepthDims;

	//PointCloud
	struct CloudPoint
	{
		vec3 PPosition;
		vec2 PUV;
		float PSize;
		CloudPoint(vec3 pPos, vec2 pUV, float pSize) :
			PPosition(pPos),
			PUV(pUV),
			PSize(pSize)
		{}
	};

	struct CloudParticle
	{
		vec3 PPosition;
		float PSize;
		float PSize_0;
		ColorA PColor;
		ColorA PColor_0;
		vec3 PAccel;
		float PGrav;
		int PAge;
		int PLife;

		CloudParticle(vec3 pPos, vec3 pAcc, float pSize, int pLife, ColorA pColor) : 
			PPosition(pPos),
			PAccel(pAcc),
			PSize(pSize),
			PAge(pLife),
			PLife(pLife),
			PColor(pColor),
			PColor_0(pColor),
			PSize_0(pSize)
		{
			PGrav = pAcc.y*2.f;
		}

		void step(Perlin &pPerlin, int pMode, ColorA pColor0)
		{
			float cNormAge = static_cast<float>(PAge) / static_cast<float>(PLife);

			switch(pMode)
			{
				case 0:
				{
					PPosition.y -= PAccel.y;
					float cNoise = pPerlin.fBm(vec3(PPosition.x*0.005f, PPosition.y*0.01f, PPosition.z*0.005f));
					float cAngle = cNoise*15.0f;
					PPosition.x += cos(cAngle)*PAccel.x*(1.0 - cNormAge);
					PPosition.z += sin(cAngle)*PAccel.z*(1.0 - cNormAge);
					PAccel.x *= 1.001f;
					PAccel.y *= .99f;
					PAccel.z *= 1.001f;
					break;
				}
				case 1:
				{
					if (cNormAge < 0.98f)
					{
						PPosition.y += PGrav;
						PGrav *= 1.005f;
					}
					break;
				}
			}
			PAge -= 1;
			PColor = lerp<ColorA>(PColor_0, pColor0, cNormAge);

			PSize = PSize_0 * cNormAge;
		}
	};

	//Particles
	vector<CloudParticle>	mPointsParticles;
	vector<ColorA>			mColorsParticles;
	gl::VboRef				mDataInstance_P;
	geom::BufferLayout		mAttribsInstance_P;
	gl::VboMeshRef			mMesh_P;
	gl::GlslProgRef			mShader_P;
	gl::BatchRef			mBatch_P;
	Perlin					mPerlin;

	//Point Cloud 
	vector<CloudPoint>	mPointsCloud;
	gl::VboRef			mDataInstance;
	geom::BufferLayout	mAttribsInstance;
	gl::VboMeshRef		mMeshCloud;
	gl::GlslProgRef		mShaderCloud;
	gl::BatchRef		mBatchCloud;

	//Skybox
	geom::Cube				mMeshSkyBox;
	gl::TextureCubeMapRef	mTexSkyBox;
	gl::GlslProgRef			mShaderSkyBox;
	gl::BatchRef			mBatchSkyBox;
	vector<ColorA>			mColorsCloud;

	CameraPersp				mCamera;
	MayaCamUI				mMayaCam;

	//Settings
	InterfaceGlRef			mGUI;

	int						mParamCloudRes,
							mParamCloudModeIndex,
							mParamColorIndex,
							mParamPModeIndex,
							mParamSpawnCount,
							mParamMaxCount;

	float					mParamSize,
							mParamLightPosX,
							mParamLightPosY,
							mParamLightPosZ,
							mParamSpecPow,
							mParamSpecStr,
							mParamAmbStr,
							mParamReflStr;

	vector<string>			mParamCloudMode;
	vector<string>			mParamSkyBoxes;
	vector<string>			mParamColors;
	vector<string>			mParamPModes;


};

void CloudNineApp::setup()
{
	setupGUI();
	setupDS();
	setupScene();
	setupFBO();


	gl::enableDepthRead();
	gl::enableDepthWrite();
}

void CloudNineApp::setupDS()
{
	mCinderDS = CinderDSAPI::create();
	mCinderDS->init();
	mCinderDS->initDepth(FrameSize::DEPTHQVGA, 60);
	mCinderDS->initRgb(FrameSize::RGBVGA, 60);
	mCinderDS->start();

	mDepthDims = mCinderDS->getDepthSize();
	mTexRgb = gl::Texture2d::create(mCinderDS->getRgbWidth(), mCinderDS->getRgbHeight(), gl::Texture2d::Format().internalFormat(GL_RGB8));
	mChanDepth = Channel16u(mDepthDims.x, mDepthDims.y);
}

void CloudNineApp::setupScene()
{
	getWindow()->setSize(1280, 720);
	setFrameRate(60);

	//Camera
	mCamera.setPerspective(45.0f, getWindowAspectRatio(), .01, 100);
	mCamera.lookAt(vec3(0), vec3(0, 0, 1), vec3(0, 1, 0));
	mCamera.setCenterOfInterestPoint(vec3(0, 0, 1.5));
	mMayaCam.setCurrentCam(mCamera);

	//Skybox
	mTexSkyBox = gl::TextureCubeMap::create(loadImage(loadAsset("cubemap_ocean.png")), gl::TextureCubeMap::Format().internalFormat(GL_RGBA8));
	mShaderSkyBox = gl::GlslProg::create(loadAsset("skybox.vert"), loadAsset("skybox.frag"));
	mBatchSkyBox = gl::Batch::create(geom::Cube(), mShaderSkyBox);
	mBatchSkyBox->getGlslProg()->uniform("uCubeMapTex", 0);

	setupPointCloud();
	setupParticles();
}

void CloudNineApp::setupPointCloud()
{
	mPointsCloud.clear();
	ivec2 cDims = mCinderDS->getDepthSize();
	for (int dy = 0; dy < cDims.y; ++dy)
	{
		for (int dx = 0; dx < cDims.x; ++dx)
		{
			mPointsCloud.push_back(CloudPoint(vec3(0), vec2(0), 1.0f));
		}
	}

	mMeshCloud = gl::VboMesh::create(geom::Sphere().radius(0.5f));

	mDataInstance = gl::Vbo::create(GL_ARRAY_BUFFER, mPointsCloud, GL_DYNAMIC_DRAW);
	mAttribsInstance.append(geom::CUSTOM_0, 3, sizeof(CloudPoint), offsetof(CloudPoint, PPosition), 1);
	mAttribsInstance.append(geom::CUSTOM_1, 2, sizeof(CloudPoint), offsetof(CloudPoint, PUV), 1);
	mAttribsInstance.append(geom::CUSTOM_2, 1, sizeof(CloudPoint), offsetof(CloudPoint, PSize), 1);
	mMeshCloud->appendVbo(mAttribsInstance, mDataInstance);

	mShaderCloud = gl::GlslProg::create(loadAsset("cloud.vert"), loadAsset("cloud.frag"));
	mBatchCloud = gl::Batch::create(mMeshCloud, mShaderCloud, { { geom::CUSTOM_0, "iPosition" }, { geom::CUSTOM_1, "iUV" }, { geom::CUSTOM_2, "iSize" } });
	mBatchCloud->getGlslProg()->uniform("mTexCube", 0);
}

void CloudNineApp::setupParticles()
{
	mPointsParticles.clear();
	mDataInstance_P = gl::Vbo::create(GL_ARRAY_BUFFER, mPointsParticles, GL_DYNAMIC_DRAW);
	mAttribsInstance_P.append(geom::CUSTOM_3, 3, sizeof(CloudParticle), offsetof(CloudParticle, PPosition), 1);
	mAttribsInstance_P.append(geom::CUSTOM_4, 1, sizeof(CloudParticle), offsetof(CloudParticle, PSize), 1);
	mAttribsInstance_P.append(geom::CUSTOM_5, 4, sizeof(CloudParticle), offsetof(CloudParticle, PColor), 1);

	mMesh_P = gl::VboMesh::create(geom::Sphere());
	mMesh_P->appendVbo(mAttribsInstance_P, mDataInstance_P);

	mShader_P = gl::GlslProg::create(loadAsset("particle.vert"), loadAsset("particle.frag"));
	mBatch_P = gl::Batch::create(mMesh_P, mShader_P, { { geom::CUSTOM_3, "iPosition" }, { geom::CUSTOM_4, "iSize" }, { geom::CUSTOM_5, "iColor" } });
	mBatch_P->getGlslProg()->uniform("mTexCube", 0);
	
	mPerlin = Perlin();
	mColorsParticles.push_back(ColorA(ColorA8u(255,218,0,255)));
	mColorsParticles.push_back(ColorA(ColorA8u(253, 181, 19, 255)));
}

void CloudNineApp::setupFBO()
{
}

void CloudNineApp::setupGUI()
{
	mParamCloudRes = 2;
	mParamSize = 1.0f;
	mParamCloudModeIndex = 0;
	mParamCloudMode.push_back("Sphere");
	mParamCloudMode.push_back("Cube");
	mParamCloudMode.push_back("Hedron");

	mParamColorIndex = 0;
	mColorsCloud.push_back(ColorA(ColorA8u(96,160,108,255)));
	mColorsCloud.push_back(ColorA(ColorA8u(224, 0, 160,255)));
	mColorsCloud.push_back(ColorA(ColorA8u(144, 32, 144,255)));
	mColorsCloud.push_back(ColorA(ColorA8u(32, 16, 192,255)));
	mParamColors.push_back("Emerald Green");
	mParamColors.push_back("Magenta");
	mParamColors.push_back("Purple");
	mParamColors.push_back("Dark Blue");

	mParamPModeIndex = 0;
	mParamPModes.push_back("Flow");
	mParamPModes.push_back("Gravity");

	mParamLightPosX = mParamLightPosY = 500.0f;
	mParamLightPosZ = 0.0f;
	mParamSpecPow = 16.0f;
	mParamSpecStr = 1.0f;
	mParamAmbStr = 1.0f;
	mParamReflStr = 1.0f;
	mParamSpawnCount = 10.0f;
	mParamMaxCount = 10000;
	mGUI = InterfaceGl::create("Settings", ivec2(300, 300));
	mGUI->setPosition(ivec2(20));

	mGUI->addParam<int>("Cloud Res", &mParamCloudRes);
	mGUI->addParam("Cloud Mode", mParamCloudMode, &mParamCloudModeIndex);
	mGUI->addParam<float>("Point Size", &mParamSize);
	mGUI->addParam("Cloud Color", mParamColors, &mParamColorIndex);
	mGUI->addParam("Light X", &mParamLightPosX);
	mGUI->addParam("Light Y", &mParamLightPosY);
	mGUI->addParam("Light Z", &mParamLightPosZ);
	mGUI->addParam("Spec Size", &mParamSpecPow);
	mGUI->addParam("Spec Strength", &mParamSpecStr);
	mGUI->addParam("Ambient Strength", &mParamAmbStr);
	mGUI->addSeparator();
	mGUI->addParam("Particle Motion", mParamPModes, &mParamPModeIndex);
	mGUI->addParam<int>("Max Particles", &mParamMaxCount);
	mGUI->addParam<int>("Spawn Count", &mParamSpawnCount);
	mGUI->addParam<float>("Env Strength", &mParamReflStr);
	
}
void CloudNineApp::mouseDown(MouseEvent event)
{
	mMayaCam.mouseDown(event.getPos());
}

void CloudNineApp::mouseDrag(MouseEvent event)
{
	mMayaCam.mouseDrag(event.getPos(), event.isLeftDown(), false, event.isRightDown());
}

void CloudNineApp::update()
{
	updatePointCloud();
	updateParticles();
}

void CloudNineApp::updatePointCloud()
{
	mCinderDS->update();
	mTexRgb->update(mCinderDS->getRgbFrame());

	mChanDepth = mCinderDS->getDepthFrame();
	Channel16u::Iter cIter = mChanDepth.getIter();

	mPointsCloud.clear();
	while (cIter.line())
	{
		while (cIter.pixel())
		{
			if (cIter.x() % mParamCloudRes == 0 && cIter.y() % mParamCloudRes == 0)
			{
				float cVal = (float)cIter.v();
				float cX = cIter.x();
				float cY = cIter.y();
				if (cVal > S_MIN_Z && cVal < S_MAX_Z)
				{
					vec3 cWorld = mCinderDS->getDepthSpacePoint(cX,cY,cVal);
					vec2 cUV = mCinderDS->getColorCoordsFromDepthSpace(cWorld);
					mPointsCloud.push_back(CloudPoint(cWorld, cUV, mParamSize));
				}
			}
		}
	}

	switch (mParamCloudModeIndex)
	{
	case 0:
		mMeshCloud = gl::VboMesh::create(geom::Sphere());
		break;
	case 1:
		mMeshCloud = gl::VboMesh::create(geom::Cube());
		break;
	case 2:
		mMeshCloud = gl::VboMesh::create(geom::Icosahedron());
		break;
	}

	mDataInstance->bufferData(mPointsCloud.size()*sizeof(CloudPoint), mPointsCloud.data(), GL_DYNAMIC_DRAW);
	mMeshCloud->appendVbo(mAttribsInstance, mDataInstance);
	mBatchCloud->replaceVboMesh(mMeshCloud);
}

void CloudNineApp::updateParticles()
{
	for (int sp = 0; sp < mParamSpawnCount; ++sp)
	{
		if (mPointsParticles.size() < mParamMaxCount)
		{
			int cX = randInt(0, mDepthDims.x);
			int cY = randInt(0, mDepthDims.y);

			if (cX%mParamCloudRes == 0 && cY%mParamCloudRes == 0)
			{
				float cZ = (float)mChanDepth.getValue(ivec2(cX, cY));
				if (cZ > S_MIN_Z&&cZ < S_MAX_Z)
				{
					vec3 cWorld = mCinderDS->getDepthSpacePoint(static_cast<float>(cX),
						static_cast<float>(cY),
						static_cast<float>(cZ));

					// pos, acc size life
					vec3 cAcc(randFloat(-5.f, 5.f), randFloat(0.5f, 3.5f), randFloat(-5.f, 5.f));
					float cSize = randFloat(0.5f, 1.0f);
					int cLife = randInt(90, 240);
					//ColorA cColor = mColorsParticles[sp % 2];
					ColorA cColor = ColorA::white();
					
					mPointsParticles.push_back(CloudParticle(cWorld, cAcc, cSize, cLife, cColor));
				}
			}
		}
	}

	//now update particles
	for (auto pit = mPointsParticles.begin(); pit != mPointsParticles.end();)
	{
		if (pit->PAge == 0)
			pit = mPointsParticles.erase(pit);
		else
		{
			pit->step(mPerlin, mParamPModeIndex, mColorsCloud[mParamColorIndex]);
			++pit;
		}
	}
	
	mDataInstance_P->bufferData(mPointsParticles.size()*sizeof(CloudParticle), mPointsParticles.data(), GL_DYNAMIC_DRAW);
	mMesh_P = gl::VboMesh::create(geom::Sphere());
	mMesh_P->appendVbo(mAttribsInstance_P, mDataInstance_P);
	mBatch_P->replaceVboMesh(mMesh_P);
}

void CloudNineApp::updateFBO()
{

}

void CloudNineApp::draw()
{
	gl::clear(Color(0, 0, 0));
	gl::setMatrices(mMayaCam.getCamera());

	//drawSkyBox();
	drawPointCloud();
	drawParticles();
	drawGUI();

}

void CloudNineApp::drawSkyBox()
{
	gl::pushMatrices();
	gl::scale(vec3(50));
	gl::ScopedTextureBind cTexSkyBox(mTexSkyBox);
	mBatchSkyBox->draw();
	gl::popMatrices();
}

void CloudNineApp::drawPointCloud()
{
	ColorA cColor = mColorsCloud[mParamColorIndex];
	mBatchCloud->getGlslProg()->uniform("ViewDirection", mMayaCam.getCamera().getViewDirection());
	mBatchCloud->getGlslProg()->uniform("LightPosition", vec3(mParamLightPosX, mParamLightPosY, mParamLightPosZ));
	mBatchCloud->getGlslProg()->uniform("SpecPow", mParamSpecPow);
	mBatchCloud->getGlslProg()->uniform("SpecStr", mParamSpecStr);
	mBatchCloud->getGlslProg()->uniform("AmbStr", mParamAmbStr);
	mBatchCloud->getGlslProg()->uniform("CloudColor", cColor);
	gl::pushMatrices();
	gl::scale(vec3(0.01));
	gl::pushMatrices();
	gl::scale(vec3(1, -1, 1));


	mTexSkyBox->bind(0);
	mBatchCloud->drawInstanced(mPointsCloud.size());
	mTexSkyBox->unbind();

	gl::popMatrices();
	gl::popMatrices();
}

void CloudNineApp::drawParticles()
{
	mBatch_P->getGlslProg()->uniform("ViewDirection", mMayaCam.getCamera().getViewDirection());
	mBatch_P->getGlslProg()->uniform("ViewPosition", mMayaCam.getCamera().getEyePoint());
	mBatch_P->getGlslProg()->uniform("LightPosition", vec3(mParamLightPosX, mParamLightPosY, mParamLightPosZ));
	mBatch_P->getGlslProg()->uniform("SpecPow", mParamSpecPow);
	mBatch_P->getGlslProg()->uniform("SpecStr", mParamSpecStr);
	mBatch_P->getGlslProg()->uniform("ReflStr", mParamReflStr);

	//gl::enableAlphaBlending();
	gl::pushMatrices();
	gl::scale(vec3(0.01));
	gl::pushMatrices();
	gl::scale(vec3(1, -1, 1));

	mTexSkyBox->bind(0);
	mBatch_P->drawInstanced(mPointsParticles.size());
	mTexSkyBox->unbind();

	gl::popMatrices();
	gl::popMatrices();
	//gl::disableAlphaBlending();

	console() << mMayaCam.getCamera().getEyePoint() << endl;
}

void CloudNineApp::drawGUI()
{
	gl::pushMatrices();
	gl::setMatricesWindow(getWindowSize());
	mGUI->draw();
	gl::popMatrices();
}

void CloudNineApp::cleanup()
{
	mCinderDS->stop();
}

CINDER_APP(CloudNineApp, RendererGl)
