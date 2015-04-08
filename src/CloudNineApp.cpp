/* MODES:
Rain
Thermals 2
Z_Vector(?)
Cubes
Spheres
Points
Circles and lines
Connected
Color Inversion
Periodic size
Delaunay/Voronoi
Bloom/Glow
Environment
Scanner/Plane Scanner
Outline/Edges
God Rays
Space!
Galaxy/Supernova effect
Wireframe primitives
Flow noise/transparency
Points+Primitives
Fog
Glowing Lines
Random Glowing Points
Simple/flat shading/hard edges
Glitch
Lens flare
Spherical distortion
Perlin Noise
Walkers/Tracers
Fractals/fbm
"Buildings"
"Soft Particles"
Alpha Particles/Sprites
Reflect the point cloud
Physics?
Plasma/implicit surfaces?
*/

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
#include "cinder/GeomIo.h"
#include "cinder/Camera.h"
#include "cinder/MayaCamUI.h"
#include "cinder/params/Params.h"
#include "CiDSAPI.h"

using namespace ci;
using namespace ci::app;
using namespace ci::params;
using namespace std;
using namespace CinderDS;


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
	void setupFBO();
	void setupGUI();

	void updatePointCloud();
	void updateFBO();

	void drawPointCloud();
	void drawSkyBox();
	void drawGUI();

	//DSAPI
	CinderDSRef			mCinderDS;
	gl::Texture2dRef	mTexRgb;

	//PointCloud
	struct CloudPoint
	{
		vec3 PPosition;
		vec2 PUV;
		CloudPoint(vec3 pPos, vec2 pUV) : PPosition(pPos), PUV(pUV){}
	};

	vector<CloudPoint>	mPointsCloud;
	gl::VboRef			mDataInstance;
	geom::BufferLayout	mAttribsInstance;
	gl::VboMeshRef		mMeshCloud;
	gl::GlslProgRef		mShaderCloud;
	gl::BatchRef		mBatchCloud;


	//Skybox
	geom::Cube				mMeshSkyBox;
	vector<gl::TextureCubeMapRef>	mTexSkyBoxes;
	gl::GlslProgRef			mShaderSkyBox;
	gl::BatchRef			mBatchSkyBox;

	CameraPersp				mCamera;
	MayaCamUI				mMayaCam;
	InterfaceGlRef			mGUI;

	int						mParamCloudRes,
							mParamCloudModeIndex,
							mParamSkyBoxIndex;

	float					mParamSphereRadius,
							mParamCubeSize,
							mParamLightPosX,
							mParamLightPosY,
							mParamLightPosZ,
							mParamSpecPow;

	vector<string>			mParamCloudMode;
	vector<string>			mParamSkyBoxes;


};

void CloudNineApp::setup()
{

	setupDS();
	setupScene();
	setupFBO();
	setupGUI();

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
	mTexRgb = gl::Texture2d::create(mCinderDS->getRgbWidth(), mCinderDS->getRgbHeight(), gl::Texture2d::Format().internalFormat(GL_RGB8));
}

void CloudNineApp::setupScene()
{
	getWindow()->setSize(1280, 720);
	setFrameRate(60);

	//Camera
	mCamera.setPerspective(45.0f, getWindowAspectRatio(), .01, 50);
	mCamera.lookAt(vec3(0), vec3(0, 0, 1), vec3(0, 1, 0));
	mCamera.setCenterOfInterestPoint(vec3(0, 0, 1.5));
	mMayaCam.setCurrentCam(mCamera);

	//PointCloud
	mPointsCloud.clear();
	ivec2 cDims = mCinderDS->getDepthSize();
	for (int dy = 0; dy < cDims.y; ++dy)
	{
		for (int dx = 0; dx < cDims.x; ++dx)
		{
			mPointsCloud.push_back(CloudPoint(vec3(0), vec2(0)));
		}
	}

	mMeshCloud = gl::VboMesh::create(geom::Sphere().radius(0.5f));

	mDataInstance = gl::Vbo::create(GL_ARRAY_BUFFER, mPointsCloud, GL_DYNAMIC_DRAW);
	mAttribsInstance.append(geom::CUSTOM_0, 3, sizeof(CloudPoint), offsetof(CloudPoint, PPosition), 1);
	mAttribsInstance.append(geom::CUSTOM_1, 2, sizeof(CloudPoint), offsetof(CloudPoint, PUV), 1);
	mMeshCloud->appendVbo(mAttribsInstance, mDataInstance);

	mShaderCloud = gl::GlslProg::create(loadAsset("cloud.vert"), loadAsset("cloud.frag"));
	mBatchCloud = gl::Batch::create(mMeshCloud, mShaderCloud, { { geom::CUSTOM_0, "iPosition" }, { geom::CUSTOM_1, "iUV" } });
	mBatchCloud->getGlslProg()->uniform("mTexRgb", 0);
	mBatchCloud->getGlslProg()->uniform("mTexCube", 1);

	//Skybox
	mTexSkyBoxes.push_back(gl::TextureCubeMap::create(loadImage(loadAsset("ph_cubemap.png")), gl::TextureCubeMap::Format().internalFormat(GL_RGBA8)));
	mTexSkyBoxes.push_back(gl::TextureCubeMap::create(loadImage(loadAsset("cubemap_nebula.png")), gl::TextureCubeMap::Format().internalFormat(GL_RGBA8)));
	mTexSkyBoxes.push_back(gl::TextureCubeMap::create(loadImage(loadAsset("cubemap_ocean.png")), gl::TextureCubeMap::Format().internalFormat(GL_RGBA8)));
	mShaderSkyBox = gl::GlslProg::create(loadAsset("skybox.vert"), loadAsset("skybox.frag"));
	mBatchSkyBox = gl::Batch::create(geom::Cube(), mShaderSkyBox);
	mBatchSkyBox->getGlslProg()->uniform("uCubeMapTex", 0);
}

void CloudNineApp::setupFBO()
{
}

void CloudNineApp::setupGUI()
{
	mParamCloudRes = 4;
	mParamSphereRadius = 0.5f;
	mParamCubeSize = 1.0f;
	mParamCloudModeIndex = 0;
	mParamCloudMode.push_back("Sphere");
	mParamCloudMode.push_back("Cube");

	mParamSkyBoxIndex = 0;
	mParamSkyBoxes.push_back("RGB Cube");
	mParamSkyBoxes.push_back("Nebula");
	mParamSkyBoxes.push_back("Ocean");

	mParamLightPosX = mParamLightPosY = mParamLightPosZ = 0.0f;
	mParamSpecPow = 8.0f;
	mGUI = InterfaceGl::create("Settings", ivec2(300, 300));
	mGUI->setPosition(ivec2(20));

	mGUI->addParam<int>("Cloud Res", &mParamCloudRes);
	mGUI->addParam("Cloud Mode", mParamCloudMode, &mParamCloudModeIndex);
	mGUI->addParam<float>("Sphere Radius", &mParamSphereRadius);
	mGUI->addParam<float>("Cube Size", &mParamCubeSize);
	mGUI->addParam("Sky Box", mParamSkyBoxes, &mParamSkyBoxIndex);
	mGUI->addParam("Light X", &mParamLightPosX);
	mGUI->addParam("Light Y", &mParamLightPosY);
	mGUI->addParam("Light Z", &mParamLightPosZ);
	mGUI->addParam("Specular", &mParamSpecPow);
	
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
}

void CloudNineApp::updatePointCloud()
{
	mCinderDS->update();
	mTexRgb->update(mCinderDS->getRgbFrame());

	Channel16u cDepth = mCinderDS->getDepthFrame();
	Channel16u::Iter cIter = cDepth.getIter();

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
				if (cVal > 0 && cVal < 1000)
				{
					vec3 cWorld = mCinderDS->getZCameraSpacePoint(cX,cY,cVal);
					vec2 cUV = mCinderDS->getColorSpaceCoordsFromZCamera(cWorld);
					mPointsCloud.push_back(CloudPoint(cWorld, cUV));
				}
			}
		}
	}

	if (mParamCloudModeIndex==0)
		mMeshCloud = gl::VboMesh::create(geom::Sphere().radius(mParamSphereRadius));
	else if (mParamCloudModeIndex == 1)
		mMeshCloud = gl::VboMesh::create(geom::Cube().size(vec3(mParamCubeSize)));
	mDataInstance->bufferData(mPointsCloud.size()*sizeof(CloudPoint), mPointsCloud.data(), GL_DYNAMIC_DRAW);
	mMeshCloud->appendVbo(mAttribsInstance, mDataInstance);
	mBatchCloud->replaceVboMesh(mMeshCloud);
}

void CloudNineApp::updateFBO()
{

}

void CloudNineApp::draw()
{
	gl::clear(Color(0, 0, 0));
	gl::setMatrices(mMayaCam.getCamera());

	drawSkyBox();
	drawPointCloud();
	drawGUI();

}

void CloudNineApp::drawSkyBox()
{
	gl::pushMatrices();
	gl::scale(vec3(20));
	gl::ScopedTextureBind cTexSkyBox(mTexSkyBoxes.at(mParamSkyBoxIndex));
	mBatchSkyBox->draw();
	gl::popMatrices();
}

void CloudNineApp::drawPointCloud()
{
	mBatchCloud->getGlslProg()->uniform("ViewDirection", mMayaCam.getCamera().getViewDirection());
	mBatchCloud->getGlslProg()->uniform("LightPosition", vec3(mParamLightPosX, mParamLightPosY, mParamLightPosZ));
	mBatchCloud->getGlslProg()->uniform("SpecPow", mParamSpecPow);
	gl::pushMatrices();
	gl::scale(vec3(0.01));
	gl::pushMatrices();
	gl::scale(vec3(1, -1, 1));
	mTexRgb->bind(0);

	gl::TextureCubeMapRef cTexSkyBox(mTexSkyBoxes.at(mParamSkyBoxIndex));
	cTexSkyBox->bind(1);
	mBatchCloud->drawInstanced(mPointsCloud.size());
	mTexRgb->unbind();
	cTexSkyBox->unbind();
	gl::popMatrices();
	gl::popMatrices();
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
