#include "GraphicsManager.h"

#include <fstream>

#include "ForwardShader.h"
#include "PostProcessShader.h"
#include "GeometryManager.h"
#include "TextureManager.h"

#include "EffectParameters.h"
#include "RenderParameters.h"

#include "FrameBufferTexture.h"
#include "RenderPass.h"

#include "RenderBatch.h"
#include "ForwardShaderState.h"
#include "Geometry.h"

static const float c_num_falloff_range = 0.0001f;	// must be greater than 0

GraphicsManager::GraphicsManager (const std::string& assetLibrary) 
	: m_forwardShader(NULL), m_postProcessShader(NULL), m_geometryManager(NULL), m_textureManager(NULL), m_assetLibrary(assetLibrary)
{
	ReloadAssets();

	glClearColor (0.0f, 0.0f, 1.0f, 1.0f);
}

GraphicsManager::~GraphicsManager () {
	ClearAssets();
}

void GraphicsManager::ClearAssets () {
	if (m_forwardShader != NULL) {
		delete m_forwardShader;
		m_forwardShader = NULL;
	}
	
	if (m_postProcessShader != NULL) {
		delete m_postProcessShader;
		m_postProcessShader = NULL;
	}

	if (m_geometryManager != NULL) {
		delete m_geometryManager;
		m_geometryManager = NULL;
	}
	
	if (m_textureManager != NULL) {
		delete m_textureManager;
		m_textureManager = NULL;
	}
}

void GraphicsManager::ReloadAssets () {
	ClearAssets();
	
	std::ifstream is;
	is.open (m_assetLibrary.c_str(), std::ios::binary);

	if(!is.is_open()) {
		printf("GraphicsManager::ReloadAssets: Error opening asset library file.");
	}
	else {
		std::string effectFile;
		std::string geometryLibrary;
		std::string textureLibrary;
   
		is >> effectFile;
		is >> geometryLibrary;
		is >> textureLibrary;

		is.close();

		glGenFramebuffers(1, &m_fbo);  

		m_geometryManager = new GeometryManager(geometryLibrary);
		LoadEffectFile (effectFile);
		m_textureManager = new TextureManager(textureLibrary);
   }           
}

void GraphicsManager::LoadEffectFile (const std::string& effectFile) {
	std::ifstream is;
	is.open (effectFile.c_str(), std::ios::binary);

	if(!is.is_open()) {
		printf("GraphicsManager::LoadEffectFile: Error opening effect file.");
		return;
	}

	while (is.good()) {
		std::string header;
		is >> header;      

		if (header == "shaders") {
			int numSections = 0;
			is >> numSections;

			while (numSections--) {
				std::string shaderName;
				std::string vertexShader;
				std::string fragmentShader;

				is >> shaderName >> vertexShader >> fragmentShader;

				// Only supports staticly defined shaders
				if (shaderName == "forward") {
					if (m_forwardShader != NULL)
						delete m_forwardShader;
					m_forwardShader = new ForwardShader(vertexShader, fragmentShader);
				}
				else if (shaderName == "postProcess") {
					if (m_postProcessShader != NULL)
						delete m_postProcessShader;
					m_postProcessShader = new PostProcessShader(vertexShader, fragmentShader);
				}
			}
		}
		else if (header == "buffers") {
			int numSections = 0;
			is >> numSections;

			while (numSections--) {
				std::string bufferName;
				std::string bufferFormat;
				float widthRatio;
				float heightRatio;

				is >> bufferName >> bufferFormat >> widthRatio >> heightRatio;
				
				std::map<std::string, FrameBufferTexture*>::iterator iter = m_frameBufferTextures.find(bufferName);
				if (iter != m_frameBufferTextures.end())
					continue;

				FrameBufferTexture* texture = new FrameBufferTexture(bufferFormat, c_window_width * widthRatio, c_window_height * heightRatio);
				m_frameBufferTextures[bufferName] = texture;
			}
		}
		else if (header == "passes") {
			int numSections = 0;
			is >> numSections;

			while (numSections--) {
				std::string passName;
				int numOptions = 0;

				is >> passName >> numOptions;

				RenderPass renderPass;

				while (numOptions--) {
					std::string settingType;

					is >> settingType;

					if (settingType == "shader") {
						std::string shaderType;

						is >> shaderType;

						if (shaderType == "forward") {
							renderPass.m_shaderType = e_ShaderTypeForward;
						}
						else if (shaderType == "postProcess") {
							renderPass.m_shaderType = e_ShaderTypePostProcess;
						}
					}
					else if (settingType == "colorAttach0") {
						is >> renderPass.m_colorAttach0;
					}
					else if (settingType == "depthAttach") {
						is >> renderPass.m_depthAttach;
					}
					else if (settingType == "source0") {
						is >> renderPass.m_source0;
					}
					else if (settingType == "source1") {
						is >> renderPass.m_source1;
					}
					else if (settingType == "flags") {
						int numFlags = 0;

						is >> numFlags;

						while (numFlags--) {
							std::string flagName;

							is >> flagName;

							renderPass.m_flags.push_back(flagName);
						}
					}
				}

				m_renderPasses.push_back(renderPass);
			}
		}
	}
}

void GraphicsManager::InitRenderBuffers () {
	/*
	// Generate FBO depth buffer
	glGenRenderbuffers(1, &m_fboDepth);   
	glBindRenderbuffer(GL_RENDERBUFFER_EXT, m_fboDepth);  
	glRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, c_window_width, c_window_height); 
	glFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_fboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER_EXT, 0);  

	// Generate FBO color buffer
	glGenTextures(1, &m_fboColor); 
	glBindTexture(GL_TEXTURE_2D, m_fboColor);  
  
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, c_window_width, c_window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); 
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  

	// Generate FBO color buffer
	glGenTextures(1, &m_fboColorHalf); 
	glBindTexture(GL_TEXTURE_2D, m_fboColorHalf);  
  
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, c_window_width / 2, c_window_height / 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); 
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  

	// Generate FBO color buffer
	glGenTextures(1, &m_fboColorBlurX); 
	glBindTexture(GL_TEXTURE_2D, m_fboColorBlurX);  
  
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, c_window_width / 2, c_window_height / 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); 
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  

	// Generate FBO color buffer
	glGenTextures(1, &m_fboColorBlurXY); 
	glBindTexture(GL_TEXTURE_2D, m_fboColorBlurXY);  
  
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, c_window_width / 2, c_window_height / 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); 
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  
	glBindTexture(GL_TEXTURE_2D, 0);  

	// Generate FBO
	glGenFramebuffers(1, &m_fbo);  
	glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_fbo); 
	glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_fboColor, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_fboDepth);
	glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT); 
  
	if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {  
		std::cout << "Couldn't create frame buffer" << std::endl; 
		exit(0); 
	}  
	*/
}

void GraphicsManager::ClearScreen () {
	m_renderBatches.clear();
	/*
	// Clear HDR FBO
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo); 
	glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_fboColor, 0);
	
	glEnable(GL_DEPTH_TEST);
	glClearColor (0.0f, 0.0f, 1.0f, 1.0f); // Set the clear colour 
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the depth and colour buffers 
	*/
}

void GraphicsManager::Render (const RenderBatch& batch) {
	m_renderBatches.push_back(batch);
	/*
	if (m_forwardShader == NULL)
		return;

	// Forward render into HDR FBO
	ForwardShaderState state = CalculateForwardShaderState(batch.m_effectParameters);
	m_forwardShader->Apply();
	m_forwardShader->SetShaderState(state);

	if (state.b_useDiffuseTexture)
		m_textureManager->SetTexture(e_TextureChannelDiffuse, batch.m_effectParameters.m_diffuseTexture);

	if (state.b_useEnvironmentMap)
		m_textureManager->SetTexture(e_TextureChannelEnvMap, m_renderParameters.m_environmentMap);

	if (state.b_useNormalMap)
		m_textureManager->SetTexture(e_TextureChannelNormalMap, batch.m_effectParameters.m_normalMap);

	m_geometryManager->RenderGeometry(batch.m_geometryID);
	*/
}

void GraphicsManager::SwapBuffers () {
	if (m_forwardShader == NULL || m_postProcessShader == NULL)
		return;
	
	for (std::vector<RenderPass>::iterator passIter = m_renderPasses.begin(); passIter != m_renderPasses.end(); ++passIter) {

		unsigned int destinationWidth;
		unsigned int destinationHeight;

		// Setup FBO targets
		if (passIter->m_colorAttach0 == "screen") {	
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

			destinationWidth = c_window_width;
			destinationHeight = c_window_height;

			glDisable(GL_DEPTH_TEST);
		}
		else {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
			
			const FrameBufferTexture* colorAttach0 = GetFrameBufferTexture(passIter->m_colorAttach0);
			const FrameBufferTexture* depthAttach = GetFrameBufferTexture(passIter->m_depthAttach);

			if (colorAttach0 == NULL) {
				printf("GraphicsManager::SwapBuffers: Invalid destination buffer %s\n", passIter->m_colorAttach0.c_str());
				return;
			}
			else {
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorAttach0->GetBufferTextureID(), 0);

				destinationWidth = colorAttach0->GetBufferTextureWidth();
				destinationHeight = colorAttach0->GetBufferTextureHeight();
			}

			if (depthAttach == NULL) {
				glDisable(GL_DEPTH_TEST);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			}
			else {
				glEnable(GL_DEPTH_TEST);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthAttach->GetBufferTextureID(), 0);	
			}
		}

		// Setup Render Viewport to the full destination size
		glViewport(0, 0, destinationWidth, destinationHeight);

		// Setup source textures
		const FrameBufferTexture* source0 = GetFrameBufferTexture(passIter->m_source0);
		const FrameBufferTexture* source1 = GetFrameBufferTexture(passIter->m_source1);
		
		if (source0 != NULL) {
			glActiveTexture(e_TextureChannelRenderPassSource0);
			glBindTexture(GL_TEXTURE_2D, source0->GetBufferTextureID());
		}	

		if (source1 != NULL) {
			glActiveTexture(e_TextureChannelRenderPassSource1);
			glBindTexture(GL_TEXTURE_2D, source1->GetBufferTextureID());
		}

		// Process RenderPass flags
		unsigned int clearFlags = 0;
		std::vector<std::string> shaderStateFlags;
		for (std::vector<std::string>::iterator flagIter = passIter->m_flags.begin(); flagIter != passIter->m_flags.end(); ++flagIter) {
			if (*flagIter == "clearColor") {
				clearFlags |= GL_COLOR_BUFFER_BIT;
			}
			else if (*flagIter == "clearDepth") {
				clearFlags |= GL_DEPTH_BUFFER_BIT;
			}
			// Let shader state try to handle the flag
			else {
				shaderStateFlags.push_back(*flagIter);
			}
		}

		// Clear buffers specified in flags
		if (clearFlags != 0) 
			glClear(clearFlags);
			
		// Render the geometry
		switch (passIter->m_shaderType) {
			case e_ShaderTypeForward:
				m_forwardShader->Apply();

				for (std::vector<RenderBatch>::iterator batchesIter = m_renderBatches.begin(); batchesIter != m_renderBatches.end(); ++batchesIter) {
					ForwardShaderState forwardShaderState = CalculateForwardShaderState(batchesIter->m_effectParameters);
					m_forwardShader->SetShaderState(forwardShaderState);

					if (forwardShaderState.b_useDiffuseTexture)
						m_textureManager->SetTexture(e_TextureChannelDiffuse, batchesIter->m_effectParameters.m_diffuseTexture);
						
					if (forwardShaderState.b_useEnvironmentMap)
						m_textureManager->SetTexture(e_TextureChannelEnvMap, m_renderParameters.m_environmentMap);
			
					if (forwardShaderState.b_useNormalMap)
						m_textureManager->SetTexture(e_TextureChannelNormalMap, batchesIter->m_effectParameters.m_normalMap);

					forwardShaderState.HandleShaderFlags(shaderStateFlags);
					m_geometryManager->RenderGeometry(batchesIter->m_geometryID);
				}
			break;
	
			case e_ShaderTypePostProcess:
				m_postProcessShader->Apply();

				PostProcessShaderState postProcessShaderState;
				postProcessShaderState.HandleShaderFlags(shaderStateFlags);

				m_postProcessShader->SetShaderState(postProcessShaderState);

				m_geometryManager->RenderGeometry("screenQuad");
			break;		
		};

	}
	

/*
	glDisable(GL_DEPTH_TEST);

	glViewport(0, 0, c_window_width / 2, c_window_height / 2);
	glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_fbo); 
	glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_fboColorHalf, 0);

	m_postProcessShader->Apply();
	glActiveTexture(e_TextureChannelRenderPassSource0);
	glBindTexture(GL_TEXTURE_2D, m_fboColor);
	m_geometryManager->RenderGeometry("screenQuad");
	
	glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_fbo); 
	glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_fboColorBlurX, 0);

	m_postProcessShader->Apply();
	glActiveTexture(e_TextureChannelRenderPassSource0);
	glBindTexture(GL_TEXTURE_2D, m_fboColorHalf);
	m_geometryManager->RenderGeometry("screenQuad");
	
	glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_fboColorBlurXY, 0);

	m_postProcessShader->Apply();
	glActiveTexture(e_TextureChannelRenderPassSource0);
	glBindTexture(GL_TEXTURE_2D, m_fboColorBlurX);
	m_geometryManager->RenderGeometry("screenQuad");
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); 
	glClear(GL_DEPTH_BUFFER_BIT);
	
	m_postProcessShader->Apply();

	glActiveTexture(e_TextureChannelRenderPassSource0);
	glBindTexture(GL_TEXTURE_2D, m_fboColor);
	
	glActiveTexture(e_TextureChannelRenderPassSource1);
	glBindTexture(GL_TEXTURE_2D, m_fboColorBlurXY);

	glViewport(0, 0, c_window_width, c_window_height);

	m_geometryManager->RenderGeometry("screenQuad");
	*/
	glutSwapBuffers();
}

ForwardShaderState GraphicsManager::CalculateForwardShaderState (const EffectParameters& effectParameters) {
	ForwardShaderState state;

	state.m_projectionMatrix = m_renderParameters.m_projectionMatrix;
	state.m_modelviewMatrix = effectParameters.m_modelviewMatrix;

	state.b_useDiffuseTexture = m_textureManager->HasTexture(effectParameters.m_diffuseTexture);
	state.b_useEnvironmentMap = m_textureManager->HasTexture(m_renderParameters.m_environmentMap);
	state.b_useNormalMap = m_textureManager->HasTexture(effectParameters.m_normalMap);

	state.m_eyePosition = m_renderParameters.m_eyePosition;

	state.m_lightDirection = m_renderParameters.m_lightDirection;
	state.m_lightCombinedAmbient = m_renderParameters.m_lightAmbient * effectParameters.m_materialAmbient;
	state.m_lightCombinedDiffuse = m_renderParameters.m_lightDiffuse * effectParameters.m_materialDiffuse;
	state.m_lightCombinedSpecular = m_renderParameters.m_lightSpecular * effectParameters.m_materialSpecular;
	state.m_materialSpecularExponent = effectParameters.m_materialSpecularExponent;
	state.m_materialGloss = effectParameters.m_materialGloss;

	for (int i = 0; i < c_num_point_lights; ++i) {
		state.m_pointLightCombinedAmbient[i] = m_renderParameters.m_pointLightAmbient[i] * effectParameters.m_materialAmbient;
		state.m_pointLightCombinedDiffuse[i] = m_renderParameters.m_pointLightDiffuse[i] * effectParameters.m_materialDiffuse;
		state.m_pointLightCombinedSpecular[i] = m_renderParameters.m_pointLightSpecular[i] * effectParameters.m_materialSpecular;

		state.b_usePointLight[i] = state.m_pointLightCombinedAmbient[i] != vec3() ||
								   state.m_pointLightCombinedDiffuse[i] != vec3() ||
								   state.m_pointLightCombinedSpecular[i] != vec3();

		if (state.b_usePointLight[i] == false)
			continue;

		state.m_pointLightPosition[i] = m_renderParameters.m_pointLightPosition[i];
		state.m_pointLightRange[i] = m_renderParameters.m_pointLightRange[i];

		float falloffRange = m_renderParameters.m_pointLightRange[i] - m_renderParameters.m_pointLightFalloff[i];
		if (falloffRange < c_num_falloff_range)
			falloffRange = c_num_falloff_range;

		state.m_pointLightAttenuationMultiplier[i] = 1.0f / falloffRange;
	}

	return state;
}

PostProcessShaderState GraphicsManager::CalculatePostProcessShaderState (const EffectParameters& effectParameters) {
	return PostProcessShaderState();
}

const FrameBufferTexture* GraphicsManager::GetFrameBufferTexture (const std::string& frameBufferTextureName) {
	std::map<std::string, FrameBufferTexture*>::iterator iter = m_frameBufferTextures.find(frameBufferTextureName);

	if (iter == m_frameBufferTextures.end())
		return NULL;
	else
		return iter->second;
}
