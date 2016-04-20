#include "cinder/app/App.h"
#include "cinder/CameraUi.h"
#include "metal.h"
#include "Batch.h"
#include "Shader.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class StockShaderApp : public App
{
public:
    
    void setup() override;
    void resize() override;
    void update() override;
    void draw() override;
    
    mtl::RenderPassDescriptorRef mRenderDescriptor;
    
    CameraPersp mCam;
    CameraUi mCamUI;
    
    mtl::BatchRef mBatchStockBasic;
    mtl::BatchRef mBatchStockTexture;
    mtl::BatchRef mBatchStockLambert;
    mtl::BatchRef mBatchStockWire;
    mtl::TextureBufferRef mTextureLogo;
};

void StockShaderApp::setup()
{
    mRenderDescriptor = mtl::RenderPassDescriptor::create();
    
    mCam.lookAt(vec3(0,0,7), vec3(0));
    mCamUI = CameraUi(&mCam, getWindow());
    
    ci::mtl::RenderPipelineStateRef renderPipelineBasic = mtl::PipelineBuilder::buildPipeline();
    mBatchStockBasic = mtl::Batch::create(ci::geom::Rect(Rectf(-0.5f,-0.5f,0.5f,0.5f)), renderPipelineBasic);
    
    ci::mtl::RenderPipelineStateRef renderPipelineTexture = mtl::PipelineBuilder::buildPipeline(mtl::ShaderDef().texture());
    mBatchStockTexture = mtl::Batch::create(ci::geom::Rect(Rectf(-0.5f,-0.5f,0.5f,0.5f)), renderPipelineTexture);
    mTextureLogo = mtl::TextureBuffer::create(loadImage(getAssetPath("cinderblock.png")));

    ci::mtl::RenderPipelineStateRef renderPipelineLambert = mtl::PipelineBuilder::buildPipeline(mtl::ShaderDef().lambert());
    mBatchStockLambert = mtl::Batch::create(ci::geom::TorusKnot(), renderPipelineLambert);
    
    ci::mtl::RenderPipelineStateRef renderPipelineWire = mtl::PipelineBuilder::buildPipeline();
    mBatchStockWire = mtl::Batch::create(ci::geom::WireIcosahedron(), renderPipelineWire);
}

void StockShaderApp::resize()
{
}

void StockShaderApp::update()
{
}

void StockShaderApp::draw()
{
    mtl::ScopedRenderCommandBuffer renderBuffer;
    mtl::ScopedRenderEncoder renderEncoder = renderBuffer.scopedRenderEncoder(mRenderDescriptor);
    
    mtl::setMatrices(mCam);
    renderEncoder.enableDepth();

    {
        mtl::ScopedModelMatrix matBasic;
        mtl::translate(vec3(0,-1,0));
        mtl::color(0.25, 0.65, 1);
        renderEncoder.draw(mBatchStockBasic);
    }
    
    {
        mtl::ScopedModelMatrix matTexture;
        mtl::translate(vec3(0,1,0));
        mtl::color(1, 1, 1);
        renderEncoder.setTexture(mTextureLogo);
        renderEncoder.draw(mBatchStockTexture);
    }
    
    {
        mtl::ScopedModelMatrix matLambert;
        mtl::translate(vec3(-1,0,0));
        mtl::scale(vec3(0.5f));
        mtl::color(1, 0, 1);
        renderEncoder.draw(mBatchStockLambert);
    }

    {
        mtl::ScopedModelMatrix matWire;
        mtl::translate(vec3(1,0,0));
        mtl::scale(vec3(0.75f));
        mtl::color(1, 1, 1);
        renderEncoder.draw(mBatchStockWire);
    }
    
}

CINDER_APP( StockShaderApp, RendererMetal )