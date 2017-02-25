/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "Framework.h"
#include "Graphics/Scene/scene.h"
#include "Graphics/Scene/Editor/SceneEditor.h"
#include "Utils/Gui.h"
#include "glm/detail/func_trigonometric.hpp"
#include "Utils/OS.h"
#include "Graphics/Scene/SceneExporter.h"
#include "Graphics/Model/AnimationController.h"
#include "API/Device.h"
#include "Graphics/Model/ModelRenderer.h"
#include "Utils/Math/FalcorMath.h"
#include "Data/HostDeviceData.h"

namespace Falcor
{
    namespace
    {
        // #TODO Update variable names and strings
        const char* kActiveModelStr = "Selected Model";
        const char* kModelsStr = "Models";
        const char* kActiveInstanceStr = "Selected Instance";
        const char* kActiveAnimationStr = "Active Animation";
        const char* kModelNameStr = "Model Name";
        const char* kInstanceStr = "Instance";
        const char* kCamerasStr = "Cameras";
        const char* kActiveCameraStr = "Active Camera";
        const char* kPathsStr = "Paths";
        const char* kActivePathStr = "Selected Path";
    };

    const float SceneEditor::kCameraModelScale = 0.5f;
    const float SceneEditor::kLightModelScale = 0.3f;
    const float SceneEditor::kKeyframeModelScale = 0.3f;

    const Falcor::Gui::RadioButtonGroup SceneEditor::kGizmoSelectionButtons
    {
        { (int32_t)Gizmo::Type::Translate, "Translation", false },
        { (int32_t)Gizmo::Type::Rotate, "Rotation", true },
        { (int32_t)Gizmo::Type::Scale, "Scaling", true }
    };

    Gui::DropdownList getPathDropdownList(const Scene* pScene, bool includeDefault)
    {
        Gui::DropdownList pathList;
        static const Gui::DropdownValue kNoPathValue{ (int32_t)Scene::kNoPath, "None" };

        if (includeDefault)
        {
            pathList.push_back(kNoPathValue);
        }

        for (uint32_t i = 0; i < pScene->getPathCount(); i++)
        {
            Gui::DropdownValue value;
            value.label = pScene->getPath(i)->getName();
            value.value = i;
            pathList.push_back(value);
        }

        return pathList;
    }

    void SceneEditor::selectActiveModel(Gui* pGui)
    {
        Gui::DropdownList modelList;
        for (uint32_t i = 0; i < mpScene->getModelCount(); i++)
        {
            Gui::DropdownValue value;
            value.label = mpScene->getModel(i)->getName();
            value.value = i;
            modelList.push_back(value);
        }

        if (pGui->addDropdown(kActiveModelStr, modelList, mSelectedModel))
        {
            mSelectedModelInstance = 0;
        }
    }

    void SceneEditor::setModelName(Gui* pGui)
    {
        char modelName[1024];
        strcpy_s(modelName, mpScene->getModel(mSelectedModel)->getName().c_str());
        if (pGui->addTextBox(kModelNameStr, modelName, arraysize(modelName)))
        {
            mpScene->getModel(mSelectedModel)->setName(modelName);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setModelVisible(Gui* pGui)
    {
        const Scene::ModelInstance::SharedPtr& instance = mpScene->getModelInstance(mSelectedModel, mSelectedModelInstance);
        bool visible = instance->isVisible();
        if (pGui->addCheckBox("Visible", visible))
        {
            instance->setVisible(visible);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setCameraFOV(Gui* pGui)
    {
        float fovY = degrees(mpScene->getActiveCamera()->getFovY());
        if (pGui->addFloatVar("FovY", fovY, 0, 360))
        {
            mpScene->getActiveCamera()->setFovY(radians(fovY));
            mSceneDirty = true;
        }
    }

    void SceneEditor::setCameraAspectRatio(Gui* pGui)
    {
        float aspectRatio = mpScene->getActiveCamera()->getAspectRatio();
        if (pGui->addFloatVar("Aspect Ratio", aspectRatio, 0, FLT_MAX, 0.001f))
        {
            auto pCamera = mpScene->getActiveCamera();
            pCamera->setAspectRatio(aspectRatio);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setCameraDepthRange(Gui* pGui)
    {
        if (pGui->beginGroup("Depth Range"))
        {
            auto pCamera = mpScene->getActiveCamera();
            float nearPlane = pCamera->getNearPlane();
            float farPlane = pCamera->getFarPlane();
            if (pGui->addFloatVar("Near Plane", nearPlane, 0, FLT_MAX, 0.1f) || (pGui->addFloatVar("Far Plane", farPlane, 0, FLT_MAX, 0.1f)))
            {
                pCamera->setDepthRange(nearPlane, farPlane);
                mSceneDirty = true;
            }
            pGui->endGroup();
        }
    }

    void SceneEditor::selectPath(Gui* pGui)
    {
        if (mpPathEditor == nullptr)
        {
            uint32_t activePath = mSelectedPath;
            Gui::DropdownList pathList = getPathDropdownList(mpScene.get(), false);
            if (pGui->addDropdown(kActivePathStr, pathList, activePath))
            {
                mSelectedPath = activePath;
                mSceneDirty = true;
            }
        }
        else
        {
            std::string msg = kActivePathStr + std::string(": ") + mpScene->getPath(mSelectedPath)->getName();
            pGui->addText(msg.c_str());
        }
    }

    void SceneEditor::setActiveCamera(Gui* pGui)
    {
        Gui::DropdownList cameraList;
        for (uint32_t i = 0; i < mpScene->getCameraCount(); i++)
        {
            Gui::DropdownValue value;
            value.label = mpScene->getCamera(i)->getName();
            value.value = i;
            cameraList.push_back(value);
        }

        uint32_t camIndex = mpScene->getActiveCameraIndex();
        if (pGui->addDropdown(kActiveCameraStr, cameraList, camIndex))
        {
            mpScene->setActiveCamera(camIndex);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setCameraName(Gui* pGui)
    {
        char camName[1024];
        strcpy_s(camName, mpScene->getActiveCamera()->getName().c_str());
        if (pGui->addTextBox("Camera Name", camName, arraysize(camName)))
        {
            mpScene->getActiveCamera()->setName(camName);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setCameraSpeed(Gui* pGui)
    {
        float speed = mpScene->getCameraSpeed();
        if (pGui->addFloatVar("Camera Speed", speed, 0, FLT_MAX, 0.1f))
        {
            mpScene->setCameraSpeed(speed);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setAmbientIntensity(Gui* pGui)
    {
        vec3 ambientIntensity = mpScene->getAmbientIntensity();
        if (pGui->addRgbColor("Ambient intensity", ambientIntensity))
        {
            mpScene->setAmbientIntensity(ambientIntensity);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setInstanceTranslation(Gui* pGui)
    {
        auto& pInstance = mpScene->getModelInstance(mSelectedModel, mSelectedModelInstance);
        vec3 t = pInstance->getTranslation();
        if (pGui->addFloat3Var("Translation", t, -FLT_MAX, FLT_MAX))
        {
            pInstance->setTranslation(t, true);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setInstanceRotation(Gui* pGui)
    {
        vec3 r = getActiveInstanceEulerRotation();
        r = degrees(r);
        if (pGui->addFloat3Var("Rotation", r, -360, 360))
        {
            r = radians(r);
            setActiveInstanceEulerRotation(r);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setInstanceScaling(Gui* pGui)
    {
        auto& pInstance = mpScene->getModelInstance(mSelectedModel, mSelectedModelInstance);
        vec3 s = pInstance->getScaling();
        if (pGui->addFloat3Var("Scaling", s, 0, FLT_MAX))
        {
            pInstance->setScaling(s);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setCameraPosition(Gui* pGui)
    {
        auto& pCamera = mpScene->getActiveCamera();
        glm::vec3 position = pCamera->getPosition();
        if (pGui->addFloat3Var("Position", position, -FLT_MAX, FLT_MAX))
        {
            pCamera->setPosition(position);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setCameraTarget(Gui* pGui)
    {
        auto& pCamera = mpScene->getActiveCamera();
        glm::vec3 target = pCamera->getTarget();
        if (pGui->addFloat3Var("Target", target, -FLT_MAX, FLT_MAX))
        {
            pCamera->setTarget(target);
            mSceneDirty = true;
        }
    }

    void SceneEditor::setCameraUp(Gui* pGui)
    {
        auto& pCamera = mpScene->getActiveCamera();
        glm::vec3 up = pCamera->getUpVector();
        if (pGui->addFloat3Var("Up", up, -FLT_MAX, FLT_MAX))
        {
            pCamera->setUpVector(up);
            mSceneDirty = true;
        }
    }

    void SceneEditor::addPointLight(Gui* pGui)
    {
        if (pGui->addButton("Add Point Light"))
        {
            auto newLight = PointLight::create();

            const auto& pCamera = mpEditorScene->getActiveCamera();

            // Place in front of camera
            glm::vec3 forward = glm::normalize(pCamera->getTarget() - pCamera->getPosition());
            newLight->setWorldPosition(pCamera->getPosition() + forward);

            uint32_t lightID = mpScene->addLight(newLight);
            mpEditorScene->addModelInstance(mpLightModel, "Light " + std::to_string(lightID), glm::vec3(), glm::vec3(), glm::vec3(kLightModelScale));
            mSelectedLight = lightID;

            rebuildLightIDMap();

            // If this is the first light added, get its modelID
            if (mEditorLightModelID == (uint32_t)-1)
            {
                mEditorLightModelID = mpEditorScene->getModelCount() - 1;
            }

            select(mpEditorScene->getModelInstance(mEditorLightModelID, mLightIDSceneToEditor[lightID]));

            mSceneDirty = true;
        }
    }

    void SceneEditor::addDirectionalLight(Gui* pGui)
    {
        if (pGui->addButton("Add Directional Light"))
        {
            auto newLight = DirectionalLight::create();
            mpScene->addLight(newLight);

            mSceneDirty = true;
        }
    }

    void SceneEditor::saveScene()
    {
        std::string filename;
        if (saveFileDialog("Scene files\0*.fscene\0\0", filename))
        {
            SceneExporter::saveScene(filename, mpScene.get());
            mSceneDirty = false;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    //// End callbacks
    //////////////////////////////////////////////////////////////////////////

    SceneEditor::UniquePtr SceneEditor::create(const Scene::SharedPtr& pScene, const uint32_t modelLoadFlags)
    {
        return UniquePtr(new SceneEditor(pScene, modelLoadFlags));
    }

    SceneEditor::SceneEditor(const Scene::SharedPtr& pScene, uint32_t modelLoadFlags)
        : mpScene(pScene)
        , mModelLoadFlags(modelLoadFlags)
    {
        mpRenderContext = gpDevice->getRenderContext();
        mpDebugDrawer = DebugDrawer::create();

        initializeEditorRendering();
        initializeEditorObjects();

        // Copy camera transform from master scene
        const auto& pSceneCamera = mpScene->getActiveCamera();
        const auto& pEditorCamera = mpEditorScene->getActiveCamera();

        pEditorCamera->setPosition(pSceneCamera->getPosition());
        pEditorCamera->setUpVector(pSceneCamera->getUpVector());
        pEditorCamera->setTarget(pSceneCamera->getTarget());
    }

    SceneEditor::~SceneEditor()
    {
        if (mSceneDirty && mpScene)
        {
            if (msgBox("Scene changed. Do you want to save the changes?", MsgBoxType::OkCancel) == MsgBoxButton::Ok)
            {
                saveScene();
            }
        }
    }

    void SceneEditor::update(double currentTime)
    {
        mpEditorSceneRenderer->update(currentTime);
    }

    void SceneEditor::initializeEditorRendering()
    {
        auto backBufferFBO = gpDevice->getSwapChainFbo();
        const float backBufferWidth = backBufferFBO->getWidth();
        const float backBufferHeight = backBufferFBO->getHeight();

        //
        // Selection Wireframe Scene
        //

        mpSelectionGraphicsState = GraphicsState::create();

        // Rasterizer State for rendering wireframe of selected object
        RasterizerState::Desc wireFrameRSDesc;
        wireFrameRSDesc.setFillMode(RasterizerState::FillMode::Wireframe).setCullMode(RasterizerState::CullMode::None).setDepthBias(-5, 0.0f);
        mpSelectionGraphicsState->setRasterizerState(RasterizerState::create(wireFrameRSDesc));

        // Depth test
        DepthStencilState::Desc dsDesc;
        dsDesc.setDepthTest(true);
        DepthStencilState::SharedPtr depthTestDS = DepthStencilState::create(dsDesc);
        mpSelectionGraphicsState->setDepthStencilState(depthTestDS);

        // Shader
        mpColorProgram = GraphicsProgram::createFromFile("Framework/Shaders/SceneEditorVS.hlsl", "Framework/Shaders/SceneEditorPS.hlsl");
        mpColorProgramVars = GraphicsVars::create(mpColorProgram->getActiveVersion()->getReflector());
        mpSelectionGraphicsState->setProgram(mpColorProgram);

        // Selection Scene and Renderer
        mpSelectionScene = Scene::create(backBufferWidth / backBufferHeight);
        mpSelectionSceneRenderer = SceneRenderer::create(mpSelectionScene);

        //
        // Master Scene Picking
        //

        mpScenePicker = Picking::create(mpScene, backBufferWidth, backBufferHeight);

        //
        // Editor Scene and Picking
        //

        mpEditorScene = Scene::create(backBufferWidth / backBufferHeight);
        mpEditorSceneRenderer = SceneEditorRenderer::create(mpEditorScene);
        mpEditorPicker = Picking::create(mpEditorScene, backBufferWidth, backBufferHeight);

        //
        // Path Shaders
        //
        RasterizerState::Desc lineRSDesc;
        lineRSDesc.setFillMode(RasterizerState::FillMode::Solid).setCullMode(RasterizerState::CullMode::None);

        GraphicsProgram::DefineList defines;
        defines.add("DEBUG_DRAW");
        mpPathProgram = GraphicsProgram::createFromFile("Framework/Shaders/SceneEditorVS.hlsl", "Framework/Shaders/SceneEditorPS.hlsl", defines);
        mpPathProgramVars = GraphicsVars::create(mpPathProgram->getActiveVersion()->getReflector());

        mpPathGraphicsState = GraphicsState::create();
        mpPathGraphicsState->setProgram(mpPathProgram);
        mpPathGraphicsState->setDepthStencilState(depthTestDS);
        mpPathGraphicsState->setRasterizerState(RasterizerState::create(lineRSDesc));
    }

    void SceneEditor::initializeEditorObjects()
    {
        //
        // Gizmos
        //

        mGizmos[(uint32_t)Gizmo::Type::Translate] = TranslateGizmo::create(mpEditorScene, "Framework/Models/TranslateGizmo.obj");
        mGizmos[(uint32_t)Gizmo::Type::Rotate] = RotateGizmo::create(mpEditorScene, "Framework/Models/RotateGizmo.obj");
        mGizmos[(uint32_t)Gizmo::Type::Scale] = ScaleGizmo::create(mpEditorScene, "Framework/Models/ScaleGizmo.obj");

        mpEditorSceneRenderer->registerGizmos(mGizmos);
        mpEditorPicker->registerGizmos(mGizmos);

        //
        // Cameras
        //

        mpCameraModel = Model::createFromFile("Framework/Models/Camera.obj", Model::GenerateTangentSpace);

        // #TODO can a scene have no cameras?
        if (mpScene->getCameraCount() > 0)
        {
            for (uint32_t i = 0; i < mpScene->getCameraCount(); i++)
            {
                const auto& pCamera = mpScene->getCamera(i);
                mpEditorScene->addModelInstance(mpCameraModel, "Camera " + std::to_string(i), glm::vec3(), glm::vec3(), glm::vec3(kCameraModelScale));
            }

            mEditorCameraModelID = mpEditorScene->getModelCount() - 1;
        }

        //
        // Lights
        //

        mpLightModel = Model::createFromFile("Framework/Models/LightBulb.obj", Model::GenerateTangentSpace);

        bool pointLightsAdded = false;
        uint32_t pointLightID = 0;
        for (uint32_t i = 0; i < mpScene->getLightCount(); i++)
        {
            const auto& pLight = mpScene->getLight(i);
            const auto& lightData = pLight->getData();

            if (pLight->getType() == LightPoint)
            {
                mpEditorScene->addModelInstance(mpLightModel, "Point Light " + std::to_string(pointLightID++), glm::vec3(), glm::vec3(), glm::vec3(kLightModelScale));
                pointLightsAdded = true;
            }
        }

        if (pointLightsAdded)
        {
            mEditorLightModelID = mpEditorScene->getModelCount() - 1;
        }

        rebuildLightIDMap();

        //
        // Master Scene Model Instance Euler Rotations
        //

        for (uint32_t modelID = 0; modelID < mpScene->getModelCount(); modelID++)
        {
            mInstanceEulerRotations.emplace_back();

            for (uint32_t instanceID = 0; instanceID < mpScene->getModelInstanceCount(modelID); instanceID++)
            {
                mInstanceEulerRotations[modelID].push_back(mpScene->getModelInstance(modelID, instanceID)->getEulerRotation());
            }
        }

        //
        // Path Attachments
        //

        for (uint32_t pathID = 0; pathID < mpScene->getPathCount(); pathID++)
        {
            const auto& pPath = mpScene->getPath(pathID);
            for (uint32_t i = 0; i < pPath->getAttachedObjectCount(); i++)
            {
                mObjToPathMap[pPath->getAttachedObject(i).get()] = pPath;
            }
        }

        mpKeyframeModel = Model::createFromFile("Framework/Models/Keyframe.obj", Model::GenerateTangentSpace);
    }

    const glm::vec3& SceneEditor::getActiveInstanceEulerRotation()
    {
        return mInstanceEulerRotations[mSelectedModel][mSelectedModelInstance];
    }

    void SceneEditor::setActiveInstanceEulerRotation(const glm::vec3& rotation)
    {
        mInstanceEulerRotations[mSelectedModel][mSelectedModelInstance] = rotation;
        mpScene->getModelInstance(mSelectedModel, mSelectedModelInstance)->setRotation(rotation);
        mSceneDirty = true;
    }

    void SceneEditor::render()
    {
        Camera *pCamera = mpEditorScene->getActiveCamera().get();

        // Draw to same Fbo that was set before this call
        mpSelectionGraphicsState->setFbo(mpRenderContext->getGraphicsState()->getFbo());

        //
        // Rendered selected model wireframe
        //

        if (mSelectedInstances.empty() == false)
        {
            mpRenderContext->setGraphicsState(mpSelectionGraphicsState);
            mpColorProgramVars["ConstColorCB"]["gColor"] = glm::vec3(0.25f, 1.0f, 0.63f);

            mpRenderContext->setGraphicsVars(mpColorProgramVars);
            mpSelectionSceneRenderer->renderScene(mpRenderContext.get(), pCamera);
        }

        //
        // Camera/Light Models, and Gizmos
        //

        updateEditorObjectTransforms();
        mpEditorSceneRenderer->renderScene(mpRenderContext.get(), pCamera);

        //
        // Paths
        //

        if (mpPathEditor != nullptr || mRenderAllPaths)
        {
            renderPath();
        }
    }

    void SceneEditor::updateEditorObjectTransforms()
    {
        // Update Gizmo model
        if (mSelectedInstances.empty() == false)
        {
            const auto& activeInstance = mpSelectionScene->getModelInstance(0, 0);
            mGizmos[(uint32_t)mActiveGizmoType]->setTransform(mpEditorScene->getActiveCamera(), activeInstance);
        }

        // Update camera model transforms
        for (uint32_t i = 0; i < mpScene->getCameraCount(); i++)
        {
            updateCameraModelTransform(i);
        }

        // Update light model transforms if any exist
        if (mEditorLightModelID != (uint32_t)-1)
        {
            for (uint32_t i = 0; i < mpEditorScene->getModelInstanceCount(mEditorLightModelID); i++)
            {
                const auto& pLight = mpScene->getLight(mLightIDEditorToScene[i]);
                auto& pModelInstance = mpEditorScene->getModelInstance(mEditorLightModelID, i);
                pModelInstance->setTranslation(pLight->getData().worldPos, true);
            }
        }
    }

    void SceneEditor::updateCameraModelTransform(uint32_t cameraID)
    {
        const auto& pCamera = mpScene->getCamera(cameraID);
        auto& pInstance = mpEditorScene->getModelInstance(mEditorCameraModelID, cameraID);

        pInstance->setTranslation(pCamera->getPosition(), false);
        pInstance->setTarget(pCamera->getTarget());
        pInstance->setUpVector(pCamera->getUpVector());
    }

    void SceneEditor::renderPath()
    {
        mpDebugDrawer->setColor(glm::vec3(0.25f, 1.0f, 0.63f));

        if (mRenderAllPaths)
        {
            for (uint32_t i = 0; i < mpScene->getPathCount(); i++)
            {
                mpDebugDrawer->addPath(mpScene->getPath(i));
            }
        }
        else if(mpPathEditor != nullptr)
        {
            mpDebugDrawer->addPath(mpPathEditor->getPath());
        }

        mpPathGraphicsState->setFbo(mpRenderContext->getGraphicsState()->getFbo());
        mpRenderContext->setGraphicsState(mpPathGraphicsState);
        mpRenderContext->setGraphicsVars(mpPathProgramVars);

        mpDebugDrawer->render(mpRenderContext.get(), mpEditorScene->getActiveCamera().get());
    }

    void SceneEditor::rebuildLightIDMap()
    {
        mLightIDEditorToScene.clear();
        mLightIDSceneToEditor.clear();

        uint32_t pointLightID = 0;
        for (uint32_t sceneLightID = 0; sceneLightID < mpScene->getLightCount(); sceneLightID++)
        {
            const auto& pLight = mpScene->getLight(sceneLightID);

            if (pLight->getType() == LightPoint)
            {
                mLightIDEditorToScene[pointLightID] = sceneLightID;
                mLightIDSceneToEditor[sceneLightID] = pointLightID;

                pointLightID++;
            }
        }
    }

    void SceneEditor::applyGizmoTransform()
    {
        const auto& activeGizmo = mGizmos[(uint32_t)mActiveGizmoType];

        switch (mSelectedObjectType)
        {
        case ObjectType::Model:
        {
            auto& pInstance = mpScene->getModelInstance(mSelectedModel, mSelectedModelInstance);
            activeGizmo->applyDelta(pInstance);

            if (mActiveGizmoType == Gizmo::Type::Rotate)
            {
                mInstanceEulerRotations[mSelectedModel][mSelectedModelInstance] = pInstance->getEulerRotation();
            }
            break;
        }

        case ObjectType::Camera:
            activeGizmo->applyDelta(mpScene->getActiveCamera());
            updateCameraModelTransform(mpScene->getActiveCameraIndex());
            break;

        case ObjectType::Light:
        {
            auto pPointLight = std::dynamic_pointer_cast<PointLight>(mpScene->getLight(mSelectedLight));
            if (pPointLight != nullptr)
            {
                activeGizmo->applyDelta(pPointLight);
                mpEditorScene->getModelInstance(mEditorLightModelID, mLightIDSceneToEditor[mSelectedLight])->setTranslation(pPointLight->getWorldPosition(), true);
            }
            break;
        }

        case ObjectType::Keyframe:
            assert(mpPathEditor);
            if (mActiveGizmoType != Gizmo::Type::Scale)
            {
                const uint32_t activeFrame = mpPathEditor->getActiveFrame();
                auto& pInstance = mpEditorScene->getModelInstance(mEditorKeyframeModelID, activeFrame);
                activeGizmo->applyDelta(pInstance);
                
                auto& pPath = mpScene->getPath(mSelectedPath);
                pPath->setFramePosition(activeFrame, pInstance->getTranslation());
                pPath->setFrameTarget(activeFrame, pInstance->getTarget());
                pPath->setFrameUp(activeFrame, pInstance->getUpVector());
            }
            break;
        }

        mSceneDirty = true;
    }

    bool SceneEditor::onMouseEvent(const MouseEvent& mouseEvent)
    {
        // Update mouse hold timer
        if (mouseEvent.type == MouseEvent::Type::LeftButtonDown || mouseEvent.type == MouseEvent::Type::LeftButtonUp)
        {
            mMouseHoldTimer.update();
        }

        //
        // Scene Editor Mouse Handler
        //

        switch (mouseEvent.type)
        {
        case MouseEvent::Type::LeftButtonDown:
            // Gizmo Selection
            if (mGizmoBeingDragged == false)
            {
                if (mpEditorPicker->pick(mpRenderContext.get(), mouseEvent.pos, mpEditorScene->getActiveCamera()))
                {
                    auto& pInstance = mpEditorPicker->getPickedModelInstance();

                    // If picked model instance is part of the active gizmo
                    if (mGizmos[(uint32_t)mActiveGizmoType]->beginAction(mpEditorScene->getActiveCamera(), pInstance))
                    {
                        mGizmoBeingDragged = true;
                        mGizmos[(uint32_t)mActiveGizmoType]->update(mpEditorScene->getActiveCamera(), mouseEvent);
                    }
                }
            }
            break;

        case MouseEvent::Type::Move:
            // Gizmo Drag
            if (mGizmoBeingDragged)
            {
                mGizmos[(uint32_t)mActiveGizmoType]->update(mpEditorScene->getActiveCamera(), mouseEvent);
                applyGizmoTransform();
            }
            break;

        case MouseEvent::Type::LeftButtonUp:
            if (mGizmoBeingDragged)
            {
                mGizmoBeingDragged = false;
            }
            else
            {
                // Scene Object Selection
                if (mMouseHoldTimer.getElapsedTime() < 0.2f)
                {
                    if (mpEditorPicker->pick(mpRenderContext.get(), mouseEvent.pos, mpEditorScene->getActiveCamera()))
                    {
                        select(mpEditorPicker->getPickedModelInstance());
                    }
                    else if (mpScenePicker->pick(mpRenderContext.get(), mouseEvent.pos, mpEditorScene->getActiveCamera()))
                    {
                        select(mpScenePicker->getPickedModelInstance());
                    }
                    else
                    {
                        deselect();
                    }
                }
            }
            break;
        }

        // Update camera
        if (mGizmoBeingDragged == false)
        {
            mpEditorSceneRenderer->onMouseEvent(mouseEvent);
        }

        return true;
    }

    bool SceneEditor::onKeyEvent(const KeyboardEvent& keyEvent)
    {
        return mpEditorSceneRenderer->onKeyEvent(keyEvent);
    }

    void SceneEditor::onResizeSwapChain()
    {
        if (mpScenePicker)
        {
            auto backBufferFBO = gpDevice->getSwapChainFbo();
            mpScenePicker->resizeFBO(backBufferFBO->getWidth(), backBufferFBO->getHeight());
        }
    }

    void SceneEditor::setActiveModelInstance(const Scene::ModelInstance::SharedPtr& pModelInstance)
    {
        for (uint32_t modelID = 0; modelID < mpScene->getModelCount(); modelID++)
        {
            // Model found, look for exact instance
            if (mpScene->getModel(modelID) == pModelInstance->getObject())
            {
                for (uint32_t instanceID = 0; instanceID < mpScene->getModelInstanceCount(modelID); instanceID++)
                {
                    // Instance found
                    if (mpScene->getModelInstance(modelID, instanceID) == pModelInstance)
                    {
                        mSelectedModel = modelID;
                        mSelectedModelInstance = instanceID;
                        return;
                    }
                }

                return;
            }
        }
    }

    void SceneEditor::renderModelElements(Gui* pGui)
    {
        if (pGui->beginGroup(kModelsStr))
        {
            addModel(pGui);
            if (mpScene->getModelCount())
            {
                deleteModel(pGui);
                if (mpScene->getModelCount() == 0)
                {
                    pGui->endGroup();
                    return;
                }

                pGui->addSeparator();
                selectActiveModel(pGui);
                setModelName(pGui);

                if (pGui->beginGroup(kInstanceStr))
                {
                    addModelInstance(pGui);
                    addModelInstanceRange(pGui);
                    deleteModelInstance(pGui);

                    if (mpScene->getModelCount() == 0)
                    {
                        pGui->endGroup();
                        return;
                    }

                    pGui->addSeparator();
                    setModelVisible(pGui);
                    setInstanceTranslation(pGui);
                    setInstanceRotation(pGui);
                    setInstanceScaling(pGui);
                    setObjectPath(pGui, mpScene->getModelInstance(mSelectedModel, mSelectedModelInstance), "ModelInstance");

                    pGui->endGroup();
                }

                renderModelAnimation(pGui);
            }
            pGui->endGroup();
        }
    }

    void SceneEditor::renderGlobalElements(Gui* pGui)
    {
        if (pGui->beginGroup("Global Settings"))
        {
            setCameraSpeed(pGui);
            setAmbientIntensity(pGui);
            pGui->endGroup();
        }
    }

    void SceneEditor::renderPathElements(Gui* pGui)
    {
        if (pGui->beginGroup(kPathsStr))
        {
            selectPath(pGui);
            // #TODO maybe add sameLine args to these helper functions instead of just changing the gui calls inside
            addPath(pGui);
            startPathEditor(pGui);
            deletePath(pGui);
            pGui->addCheckBox("Render All Paths", mRenderAllPaths);
            pGui->endGroup();
        }
    }

    void SceneEditor::renderCameraElements(Gui* pGui)
    {
        if (pGui->beginGroup(kCamerasStr))
        {
            addCamera(pGui);
            setActiveCamera(pGui);
            setCameraName(pGui);
            deleteCamera(pGui);
            pGui->addSeparator();
            assert(mpScene->getCameraCount() > 0);
            setCameraAspectRatio(pGui);
            setCameraDepthRange(pGui);

            setCameraPosition(pGui);
            setCameraTarget(pGui);
            setCameraUp(pGui);

            setObjectPath(pGui, mpScene->getActiveCamera(), "Camera");

            pGui->endGroup();
        }
    }

    void SceneEditor::renderLightElements(Gui* pGui)
    {
        if (pGui->beginGroup("Lights"))
        {
            addPointLight(pGui);
            addDirectionalLight(pGui);

            for (uint32_t i = 0; i < mpScene->getLightCount(); i++)
            {
                std::string name = mpScene->getLight(i)->getName();
                if (name.size() == 0)
                {
                    name = std::string("Light ") + std::to_string(i);
                }
                if (pGui->beginGroup(name.c_str()))
                {
                    const auto& pLight = mpScene->getLight(i);
                    pLight->setUiElements(pGui);

                    if (pLight->getType() == LightPoint)
                    {
                        setObjectPath(pGui, pLight, "PointLight");
                    }

                    if (pGui->addButton("Remove"))
                    {
                        if (msgBox("Delete light?", MsgBoxType::OkCancel) == MsgBoxButton::Ok)
                        {
                            mpScene->deleteLight(i);

                            uint32_t instanceID = mLightIDSceneToEditor[i];
                            bool isLastInstance = mpEditorScene->getModelInstanceCount(mEditorLightModelID) == 1;

                            mpEditorScene->deleteModelInstance(mEditorLightModelID, instanceID);

                            if (isLastInstance)
                            {
                                mEditorLightModelID = (uint32_t)-1;
                            }

                            rebuildLightIDMap();
                        }
                    }

                    pGui->endGroup();
                }
            }
            pGui->endGroup();
        }
    }

    void SceneEditor::renderGui(Gui* pGui)
    {
        pGui->pushWindow("Scene Editor", 400, 600, 20, 250);
        if (pGui->addButton("Export Scene"))
        {
            saveScene();
        }

        // Gizmo Selection
        int32_t selectedGizmo = (int32_t)mActiveGizmoType;
        pGui->addRadioButtons(kGizmoSelectionButtons, selectedGizmo);
        setActiveGizmo((Gizmo::Type)selectedGizmo, mSelectedInstances.size() > 0);

        pGui->addSeparator();
        renderGlobalElements(pGui);
        renderCameraElements(pGui);
        renderPathElements(pGui);
        renderModelElements(pGui);
        renderLightElements(pGui);

        pGui->popWindow();

        if (mpPathEditor)
        {
            mpPathEditor->render(pGui);
        }
    }

    void SceneEditor::renderModelAnimation(Gui* pGui)
    {
        const auto pModel = mpScene->getModelCount() ? mpScene->getModel(mSelectedModel) : nullptr;

        if (pModel && pModel->hasAnimations())
        {
            Gui::DropdownList list(pModel->getAnimationsCount() + 1);
            list[0].label = "Bind Pose";
            list[0].value = BIND_POSE_ANIMATION_ID;

            for (uint32_t i = 0; i < pModel->getAnimationsCount(); i++)
            {
                list[i + 1].value = i;
                list[i + 1].label = pModel->getAnimationName(i);
                if (list[i + 1].label.size() == 0)
                {
                    list[i + 1].label = std::to_string(i);
                }
            }
            uint32_t activeAnim = mpScene->getModel(mSelectedModel)->getActiveAnimation();
            if (pGui->addDropdown(kActiveAnimationStr, list, activeAnim)) mpScene->getModel(mSelectedModel)->setActiveAnimation(activeAnim);
        }
    }

    void SceneEditor::select(const Scene::ModelInstance::SharedPtr& pModelInstance)
    {
        // If instance has already been picked, ignore it
        if (mSelectedInstances.count(pModelInstance.get()) > 0)
        {
            return;
        }

        deselect();

        mpSelectionScene->addModelInstance(pModelInstance);

        setActiveGizmo(mActiveGizmoType, true);

        //
        // Track selection and set corresponding object as selected/active
        //

        mSelectedInstances.insert(pModelInstance.get());

        if (pModelInstance->getObject() == mpCameraModel)
        {
            mSelectedObjectType = ObjectType::Camera;

            uint32_t cameraID = findEditorModelInstanceID(mEditorCameraModelID, pModelInstance);
            if (cameraID != (uint32_t)-1)
            {
                mpScene->setActiveCamera(cameraID);
            }
        }
        else if (pModelInstance->getObject() == mpLightModel)
        {
            mSelectedObjectType = ObjectType::Light;

            uint32_t instanceID = findEditorModelInstanceID(mEditorLightModelID, pModelInstance);
            if (instanceID != (uint32_t)-1)
            {
                mSelectedLight = mLightIDEditorToScene[instanceID];
            }
        }
        else if (pModelInstance->getObject() == mpKeyframeModel)
        {
            assert(mpPathEditor);
            mSelectedObjectType = ObjectType::Keyframe;

            uint32_t frameID = findEditorModelInstanceID(mEditorKeyframeModelID, pModelInstance);
            if (frameID != (uint32_t)-1)
            {
                mpPathEditor->setActiveFrame(frameID);
            }
        }
        else
        {
            mSelectedObjectType = ObjectType::Model;
            setActiveModelInstance(pModelInstance);
        }
    }

    void SceneEditor::deselect()
    {
        if (mpSelectionScene)
        {
            mpSelectionScene->deleteAllModels();
        }

        setActiveGizmo(mActiveGizmoType, false);

        mSelectedInstances.clear();
    }

    void SceneEditor::setActiveGizmo(Gizmo::Type type, bool show)
    {
        if (mGizmos[(uint32_t)type] != nullptr)
        {
            if (mActiveGizmoType != type)
            {
                // Hide old gizmo
                mGizmos[(uint32_t)mActiveGizmoType]->setVisible(false);

                // Set visibility on new gizmo
                mGizmos[(uint32_t)type]->setVisible(show);
            }
            else
            {
                // Change visibility on active gizmo
                mGizmos[(uint32_t)mActiveGizmoType]->setVisible(show);
            }
        }

        mActiveGizmoType = type;
    }

    uint32_t SceneEditor::findEditorModelInstanceID(uint32_t modelID, const Scene::ModelInstance::SharedPtr& pInstance) const
    {
        for (uint32_t i = 0; i < mpEditorScene->getModelInstanceCount(modelID); i++)
        {
            if (mpEditorScene->getModelInstance(modelID, i) == pInstance)
            {
                return i;
            }
        }

        return (uint32_t)-1;
    }

    void SceneEditor::addModel(Gui* pGui)
    {
        if (pGui->addButton("Add Model"))
        {
            std::string filename;
            if (openFileDialog(Model::kSupportedFileFormatsStr, filename))
            {
                auto pModel = Model::createFromFile(filename, mModelLoadFlags);
                if (pModel == nullptr)
                {
                    logError("Error when trying to load model " + filename);
                    return;
                }

                // Get the model name
                size_t offset = filename.find_last_of("/\\");
                std::string modelName = (offset == std::string::npos) ? filename : filename.substr(offset + 1);

                // Remove the last extension
                offset = modelName.find_last_of(".");
                modelName = (offset == std::string::npos) ? modelName : modelName.substr(0, offset);

                pModel->setName(modelName);
                mpScene->addModelInstance(pModel, "Instance 0");

                mSelectedModel = mpScene->getModelCount() - 1;
                mSelectedModelInstance = 0;

                mInstanceEulerRotations.emplace_back();
                mInstanceEulerRotations.back().push_back(mpScene->getModelInstance(mSelectedModel, mSelectedModelInstance)->getEulerRotation());
            }
            mSceneDirty = true;
        }
    }

    void SceneEditor::deleteModel()
    {
        mpScene->deleteModel(mSelectedModel);
        mInstanceEulerRotations.erase(mInstanceEulerRotations.begin() + mSelectedModel);
        mSelectedModel = 0;
        mSelectedModelInstance = 0;
        mSceneDirty = true;
        deselect();
    }

    void SceneEditor::deleteModel(Gui* pGui)
    {
        if (mpScene->getModelCount() > 0)
        {
            if (pGui->addButton("Remove Model"))
            {
                deleteModel();
            }
        }
    }

    void SceneEditor::addModelInstance(Gui* pGui)
    {
        if (pGui->addButton("Add Instance"))
        {
            const auto& pInstance = mpScene->getModelInstance(mSelectedModel, mSelectedModelInstance);
            auto& pModel = mpScene->getModel(mSelectedModel);

            // Set new selection index
            mSelectedModelInstance = mpScene->getModelInstanceCount(mSelectedModel);

            // Add instance
            mpScene->addModelInstance(pModel, "Instance " + mSelectedModelInstance, pInstance->getTranslation(), pInstance->getEulerRotation(), pInstance->getScaling());

            auto& pNewInstance = mpScene->getModelInstance(mSelectedModel, mSelectedModelInstance);
            mInstanceEulerRotations[mSelectedModel].push_back(pNewInstance->getEulerRotation());
            select(pNewInstance);

            mSceneDirty = true;
        }
    }

    void SceneEditor::addModelInstanceRange(Gui* pGui)
    {
        pGui->addIntVar(kActiveInstanceStr, mSelectedModelInstance, 0, mpScene->getModelInstanceCount(mSelectedModel) - 1);
    }

    void SceneEditor::deleteModelInstance(Gui* pGui)
    {
        if (pGui->addButton("Remove Instance"))
        {
            if (mpScene->getModelInstanceCount(mSelectedModel) == 1)
            {
                auto MbRes = msgBox("The active model has a single instance. Removing it will remove the model from the scene.\nContinue?", MsgBoxType::OkCancel);
                if (MbRes == MsgBoxButton::Ok)
                {
                    deleteModel();
                    return;
                }
            }

            mpScene->deleteModelInstance(mSelectedModel, mSelectedModelInstance);

            auto& modelRotations = mInstanceEulerRotations[mSelectedModel];
            modelRotations.erase(modelRotations.begin() + mSelectedModelInstance);

            deselect();

            mSelectedModelInstance = 0;
            mSceneDirty = true;
        }
    }

    void SceneEditor::addCamera(Gui* pGui)
    {
        if (pGui->addButton("Add Camera"))
        {
            auto pCamera = Camera::create();
            auto pActiveCamera = mpScene->getActiveCamera();
            *pCamera = *pActiveCamera;
            pCamera->setName(pActiveCamera->getName() + "_");

            const uint32_t camIndex = mpScene->addCamera(pCamera);
            mpScene->setActiveCamera(camIndex);

            // Update editor scene
            mpEditorScene->addModelInstance(mpCameraModel, pCamera->getName(), glm::vec3(), glm::vec3(), glm::vec3(kCameraModelScale));

            // #TODO Re: initialization, can scenes start with 0 cameras?
            // If this is the first camera added, get its modelID
            if (mEditorCameraModelID == (uint32_t)-1)
            {
                mEditorCameraModelID = mpEditorScene->getModelCount() - 1;
            }

            select(mpEditorScene->getModelInstance(mEditorCameraModelID, camIndex));

            mSceneDirty = true;
        }
    }

    void SceneEditor::deleteCamera(Gui* pGui)
    {
        if (pGui->addButton("Remove Camera"))
        {
            if (mpScene->getCameraCount() == 1)
            {
                msgBox("The Scene has only one camera. Scenes must have at least one camera. Ignoring call.");
                return;
            }

            mpScene->deleteCamera(mpScene->getActiveCameraIndex());
            mpEditorScene->deleteModelInstance(mEditorCameraModelID, mpScene->getActiveCameraIndex());
            select(mpEditorScene->getModelInstance(mEditorCameraModelID, mpScene->getActiveCameraIndex()));

            mSceneDirty = true;
        }
    }

    void SceneEditor::addPath(Gui* pGui)
    {
        if (mpPathEditor == nullptr && pGui->addButton("Add Path"))
        {
            auto pPath = ObjectPath::create();
            pPath->setName("Path " + std::to_string(mpScene->getPathCount()));
            mSelectedPath = mpScene->addPath(pPath);

            startPathEditor();
            mSceneDirty = true;
        }
    }

    void SceneEditor::deletePath(Gui* pGui)
    {
        if (mpPathEditor)
        {
            // Can't delete a path while the path editor is opened
            return;
        }

        if (pGui->addButton("Delete Path", true))
        {
            auto& pPath = mpScene->getPath(mSelectedPath);
            for (uint32_t i = 0; i < pPath->getAttachedObjectCount(); i++)
            {
                mObjToPathMap.erase(pPath->getAttachedObject(i).get());
            }

            mpScene->deletePath(mSelectedPath);

            if (mSelectedPath == mpScene->getPathCount())
            {
                mSelectedPath = mpScene->getPathCount() - 1;
            }

            mSceneDirty = true;
        }
    }

    void SceneEditor::pathEditorFrameChangedCB()
    {
        const uint32_t activeFrameID = mpPathEditor->getActiveFrame();

        const auto& pKeyframeInstance = mpEditorScene->getModelInstance(mEditorKeyframeModelID, activeFrameID);
        select(pKeyframeInstance);

        const auto& frame = mpScene->getPath(mSelectedPath)->getKeyFrame(activeFrameID);
        pKeyframeInstance->setTranslation(frame.position, false);
        pKeyframeInstance->setTarget(frame.target);
        pKeyframeInstance->setUpVector(frame.up);
    }

    void SceneEditor::pathEditorFrameAddRemoveCB()
    {
        if (mSelectedObjectType == ObjectType::Keyframe)
        {
            deselect();
        }

        removeSelectedPathKeyframeModels();
        addSelectedPathKeyframeModels();
    }

    void SceneEditor::pathEditorFinishedCB()
    {
        deselect();

        if (mpPathEditor->getPath()->getKeyFrameCount() > 0)
        {
            removeSelectedPathKeyframeModels();
        }

        mpPathEditor = nullptr;
    }

    void SceneEditor::addSelectedPathKeyframeModels()
    {
        assert(mpPathEditor != nullptr);

        // Add models to represent keyframes
        const auto& pPath = mpPathEditor->getPath();

        if (pPath->getKeyFrameCount() > 0)
        {
            for (uint32_t i = 0; i < pPath->getKeyFrameCount(); i++)
            {
                const auto& frame = pPath->getKeyFrame(i);
                auto pNewInstance = Scene::ModelInstance::create(mpKeyframeModel, frame.position, frame.target, frame.up, glm::vec3(kKeyframeModelScale), "Frame " + std::to_string(i));
                mpEditorScene->addModelInstance(pNewInstance);
            }

            mEditorKeyframeModelID = mpEditorScene->getModelCount() - 1;
        }
    }

    void SceneEditor::removeSelectedPathKeyframeModels()
    {
        assert(mpPathEditor != nullptr);

        if (mpPathEditor->getPath()->getKeyFrameCount() > 0)
        {
            // Remove keyframe models
            mpEditorScene->deleteModel(mEditorKeyframeModelID);
            mEditorKeyframeModelID = (uint32_t)-1;
        }
    }

    void SceneEditor::startPathEditor()
    {
        const auto& pPath = mpScene->getPath(mSelectedPath);
        mpPathEditor = PathEditor::create(pPath, 
            [this]() { pathEditorFrameChangedCB(); },
            [this]() { pathEditorFrameAddRemoveCB(); },
            [this]() { pathEditorFinishedCB(); });

        addSelectedPathKeyframeModels();

        mSceneDirty = true;
    }

    void SceneEditor::startPathEditor(Gui* pGui)
    {
        if (mpPathEditor == nullptr)
        {
            if (pGui->addButton("Edit Path", true))
            {
                startPathEditor();
            }
        }
    }

    void SceneEditor::setObjectPath(Gui* pGui, const IMovableObject::SharedPtr& pMovable, const std::string& objType)
    {
        // Find what path this pMovable is set to, if any
        ObjectPath::SharedPtr pOldPath;
        uint32_t oldPathID = Scene::kNoPath;

        auto it = mObjToPathMap.find(pMovable.get());
        if (it != mObjToPathMap.end())
        {
            pOldPath = it->second;
        }

        // Find path ID
        if (pOldPath != nullptr)
        {
            for (uint32_t i = 0; i < mpScene->getPathCount(); i++)
            {
                if (mpScene->getPath(i) == pOldPath)
                {
                    oldPathID = i;
                }
            }
        }

        // Append tag to avoid hash collisions in imgui. ##Tag does not appear when rendered
        std::string label = std::string(kActivePathStr) + "##" + objType;

        uint32_t newPathID = oldPathID;
        if (pGui->addDropdown(label.c_str(), getPathDropdownList(mpScene.get(), true), newPathID))
        {
            // Detach from old path
            if (oldPathID != Scene::kNoPath)
            {
                pOldPath->detachObject(pMovable);
                mObjToPathMap.erase(pMovable.get());

                // #HACK Find a way to work with base/movable matrices on object instances. Cameras and Lights don't do this...
                if (std::dynamic_pointer_cast<Scene::ModelInstance>(pMovable) != nullptr)
                {
                    pMovable->move(glm::vec3(), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                }
            }

            // Attach to new path
            if (newPathID != Scene::kNoPath)
            {
                const auto& pNewPath = mpScene->getPath(newPathID);
                pNewPath->attachObject(pMovable);
                mObjToPathMap[pMovable.get()] = pNewPath;
            }

        }
    }

}
