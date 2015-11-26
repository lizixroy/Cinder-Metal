#include "cinder/app/App.h"
#include "cinder/Rand.h"
#include "cinder/Log.h"

// Cinder-Metal
#include "metal.h"
#include "VertexBuffer.h"
#include "SharedData.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace cinder::mtl;

const static int kNumInflightBuffers = 3;
const static int kNumSortStateBuffers = 91; // Must be >= the number of sort passes

typedef struct {
    vec3 position;
    vec3 velocity;
} Particle;

class ParticleSortingApp : public App {

public:
    
    ParticleSortingApp() :
    mRotation(0.f)
    ,mModelScale(2.f)
    {}
    
    void setup() override;
    void loadAssets();
    void resize() override;
    void update() override;
    void draw() override;
    void mouseDown( MouseEvent event ) override;
    void mouseDrag( MouseEvent event ) override;
    void bitonicSort( bool shouldLogOutput );
    void logComputeOutput( const myUniforms_t uniforms );
    
    DepthStateRef mDepthEnabled;
    
    myUniforms_t mUniforms;
    DataBufferRef mDynamicConstantBuffer;
    DataBufferRef mSortStateBuffer;
    uint8_t mConstantDataBufferIndex;
    
    float mRotation;
    CameraPersp mCamera;
    vec2 mMousePos;
    float mModelScale;

    // Particles
    DataBufferRef mParticlesUnsorted;
    DataBufferRef mParticleIndices;
    RenderPassDescriptorRef mRenderDescriptor;
    RenderPipelineStateRef mPipelineParticles;
    TextureBufferRef mTextureParticle;
    
    // Sort pass
    ComputePipelineStateRef mPipelineBitonicSort;
    
};

void ParticleSortingApp::setup()
{
    mConstantDataBufferIndex = 0;
    
    mDepthEnabled = DepthState::create( DepthState::Format().depthCompareFunction(7) );
    
    mRenderDescriptor = RenderPassDescriptor::create( RenderPassDescriptor::Format()
                                                      .clearColor( ColorAf(0.5f,0.f,1.f,1.f) ) );

    loadAssets();
}

void ParticleSortingApp::resize()
{
    mCamera = CameraPersp(getWindowWidth(), getWindowHeight(), 65.f, 0.1f, 100.f);
    mCamera.lookAt(vec3(0,0,-5), vec3(0));
}

void ParticleSortingApp::loadAssets()
{
    mDynamicConstantBuffer = DataBuffer::create( sizeof(myUniforms_t) * kNumInflightBuffers,
                                                 nullptr,
                                                 "Uniform Buffer" );
    
    mSortStateBuffer = DataBuffer::create( sizeof(sortState_t) * kNumSortStateBuffers,
                                           nullptr,
                                           "Sort State Buffer" );

    // Set up the particles
    vector<Particle> particles;
    vector<ivec4> indices;
    ci::Rand random;
    random.seed((UInt32)time(NULL));
    for ( unsigned int i = 0; i < kParticleDimension * kParticleDimension; ++i )
    {
        Particle p;
        p.position = random.randVec3();
        p.velocity = random.randVec3();
        particles.push_back(p);
        if ( i % 4 == 0 )
        {
            indices.push_back(ivec4(i, i+1, i+2, i+3));
        }
    }

    // Make sure we've got the right number of indicies
    assert( float(indices.size()) == particles.size() / 4.0f );
    
    mPipelineParticles = RenderPipelineState::create("vertex_particles", "fragment_point_texture",
                                                     RenderPipelineState::Format()
                                                     .blendingEnabled(true));
    
    mTextureParticle = TextureBuffer::create(loadImage(getAssetPath("particle.png")));
    
    mParticlesUnsorted = DataBuffer::create(particles);
    mParticleIndices = DataBuffer::create(indices);
    mPipelineBitonicSort = ComputePipelineState::create("bitonic_sort_by_value");
}

void ParticleSortingApp::mouseDown( MouseEvent event )
{
    mMousePos = event.getPos();
}

void ParticleSortingApp::mouseDrag( MouseEvent event )
{
    vec2 newPos = event.getPos();
    vec2 offset = newPos - mMousePos;
    mMousePos = newPos;
    mModelScale = ci::math<float>::clamp(mModelScale + (offset.x / getWindowWidth()), 1.f, 3.f);
}

void ParticleSortingApp::update()
{
    mRotation += 0.0015f;

    mat4 modelMatrix = glm::rotate(mRotation, vec3(1.0f, 1.0f, 1.0f));
    modelMatrix = glm::scale(modelMatrix, vec3(mModelScale));
    
    mat4 normalMatrix = inverse(transpose(modelMatrix));
    mat4 modelViewMatrix = mCamera.getViewMatrix() * modelMatrix;
    mat4 modelViewProjectionMatrix = mCamera.getProjectionMatrix() * modelViewMatrix;
    
    // Is there a clean way to automatically wrap these up?
    mUniforms.normalMatrix = toMtl(normalMatrix);
    mUniforms.modelViewProjectionMatrix = toMtl(modelViewProjectionMatrix);
    mUniforms.viewMatrix = toMtl(mCamera.getViewMatrix());
    mUniforms.inverseViewMatrix = toMtl(mCamera.getInverseViewMatrix());
    mUniforms.inverseModelMatrix = toMtl(inverse(modelMatrix));
    mUniforms.modelMatrix = toMtl(modelMatrix);
    mUniforms.modelViewMatrix = toMtl(modelViewMatrix);
    mDynamicConstantBuffer->setData( &mUniforms, mConstantDataBufferIndex );
    
    bitonicSort( false );
}

// NOTE: We pass in a copy of the uniforms because they may have changed
// by the time the compute is finished.
void ParticleSortingApp::logComputeOutput( const myUniforms_t uniforms )
{
    ivec4 *sortedIndices = (ivec4 *)mParticleIndices->contents();
    Particle *particles = (Particle *)mParticlesUnsorted->contents();

    CI_LOG_I("Sorted Z values:");
    for ( long i = 0; i < mUniforms.numParticles / 4; ++i )
    {
        ivec4 v = sortedIndices[i];
        for ( int j = 0; j < 4; ++j )
        {
            int index = v[j];
            Particle & p = particles[index];
            vec4 pos = fromMtl(uniforms.modelMatrix) * vec4(p.position, 0.f);
            float value = pos.z;
            printf("%f, ", value);
        }
    }
}

void ParticleSortingApp::bitonicSort( bool shouldLogOutput )
{
    int passNum = 0;
    uint constantsOffset = (sizeof(myUniforms_t) * mConstantDataBufferIndex);
    {
        uint arraySize = mUniforms.numParticles;
        int numStages = 0;
        
        // Calculate the number of stages
        for ( uint temp = arraySize; temp > 2; temp >>= 1 )
        {
            numStages++;
        }
        
        // NOTE:
        // If we log out the results while the command buffer is still running, the values might
        // be incorrect. This can be fixed by logging out in the completion handler, OR, passing
        // `true` into the ScopedCommandBuffer constructor, which causes it to wait synchronously
        // until the work is done.
        // We'll do both for demonstration.
        
        ScopedCommandBuffer commandBuffer( shouldLogOutput ); // param value indicates if we should synchrounously wait until the work is done.
        
        if ( shouldLogOutput )
        {
            myUniforms_t uniformsCopy = mUniforms;
            commandBuffer.addCompletionHandler([&]( void * mtlCommandBuffer ){
                logComputeOutput(uniformsCopy);
            });
        }
        
        ScopedComputeEncoder computeEncoder(commandBuffer());
        
        for ( int stage = 0; stage < numStages; stage++ )
        {
            for ( int passOfStage = stage; passOfStage >= 0; passOfStage-- )
            {
                sortState_t sortState;
                sortState.stage = stage;
                sortState.pass = passOfStage;
                sortState.passNum = passNum;
                sortState.direction = 1; // ascending
                mSortStateBuffer->setData(&sortState, passNum);
                
                computeEncoder()->setPipelineState( mPipelineBitonicSort );
                
                computeEncoder()->setBufferAtIndex( mParticleIndices, 1 );
                computeEncoder()->setBufferAtIndex( mParticlesUnsorted, 2 );
                computeEncoder()->setBufferAtIndex( mSortStateBuffer, 3, sizeof(sortState_t) * passNum );

                computeEncoder()->setUniforms( mDynamicConstantBuffer, constantsOffset );
                
                size_t gsz = arraySize / (2*4);
                // NOTE: work size is not 1-per vector.
                // Its the number of quad items in input array
                size_t global_work_size = passOfStage ? gsz : gsz << 1;
                
                computeEncoder()->dispatch( ivec3( global_work_size, 1, 1), ivec3( 32, 1, 1 ) );
                assert( passNum < kNumSortStateBuffers );
                passNum++;
            }
        }
    }
    
}

void ParticleSortingApp::draw()
{
    uint constantsOffset = (sizeof(myUniforms_t) * mConstantDataBufferIndex);
    
    ScopedRenderBuffer renderBuffer;
    ScopedRenderEncoder renderEncoder(renderBuffer(), mRenderDescriptor);

    // Set uniforms
    renderEncoder()->setUniforms( mDynamicConstantBuffer, constantsOffset );
    
    // Enable depth
    renderEncoder()->setDepthStencilState( mDepthEnabled );

    // Draw particles
    renderEncoder()->pushDebugGroup("Draw Particles");
    
    // Set the program
    renderEncoder()->setPipelineState( mPipelineParticles );

    // Pass in the unsorted particles
    renderEncoder()->setBufferAtIndex( mParticlesUnsorted, ciBufferIndexInterleavedVerts );

    // Pass in the sorted particle indices
    renderEncoder()->setBufferAtIndex( mParticleIndices, ciBufferIndexIndicies );

    renderEncoder()->setTexture(mTextureParticle);
    
    renderEncoder()->draw( mtl::geom::POINT, mUniforms.numParticles );

    renderEncoder()->popDebugGroup();
    
    mConstantDataBufferIndex = (mConstantDataBufferIndex + 1) % kNumInflightBuffers;
}

CINDER_APP( ParticleSortingApp,
            RendererMetal( RendererMetal::Options().numInflightBuffers(kNumInflightBuffers) ),
            []( ParticleSortingApp::Settings *settings )
            {
                // Just observe 1 touch for scaling
                settings->setMultiTouchEnabled(false);
            }
          )