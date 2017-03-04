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
#include "MaterialEditorSample.h"
#include "API/D3D/FalcorD3D.h"

Gui::DropdownList MaterialEditorSample::kModelDropdown = 
{
    {(uint32_t)DisplayModel::Sphere, "Sphere"},
    {(uint32_t)DisplayModel::Cube, "Cube"},
    {(uint32_t)DisplayModel::Teapot, "Teapot"}
};

void MaterialEditorSample::onGuiRender()
{
    if (mpGui->addButton("Load from scene file"))
    {
        if (mpMaterial == nullptr || msgBox("Are you sure?", MsgBoxType::OkCancel) == MsgBoxButton::Ok)
        {
            std::string filename;
            if (openFileDialog("Scene files\0*.fscene\0\0", filename))
            {
                mpScene = Scene::loadFromFile(filename, Model::GenerateTangentSpace);
                mMaterialSelectionState = true;
                mSelectedMaterialID = 0;
            }
        }
    }

    if (mpGui->addButton("New Material"))
    {
        if (mpMaterial == nullptr || msgBox("You will lose unsaved changes on the current material.", MsgBoxType::OkCancel) == MsgBoxButton::Ok)
        {
            mpMaterial = Material::create("New Material");
            mpMaterialEditor = MaterialEditor::create(mpMaterial, false);
        }
    }

    //
    // Preview Window to control lighting
    //

    mpGui->pushWindow("Preview", 325, 200, 290, 40);

    mpGui->addDropdown("Display Model", kModelDropdown, mActiveModel);

    if (mpGui->beginGroup("Lights"))
    {
        mpGui->addRgbColor("Ambient intensity", mAmbientIntensity);
        if (mpGui->beginGroup("Directional Light"))
        {
            mpDirLight->setUiElements(mpGui.get());
            mpGui->endGroup();
        }
        if (mpGui->beginGroup("Point Light"))
        {
            mpPointLight->setUiElements(mpGui.get());
            mpGui->endGroup();
        }
        mpGui->endGroup();
    }

    mpGui->popWindow();


    // When scene has been loaded, render UI to select material from scene file
    if (mMaterialSelectionState)
    {
        renderMaterialSelection();
    }
    else if (mpMaterialEditor != nullptr)
    {
        mpMaterialEditor->renderGui(mpGui.get());
    }
}

void MaterialEditorSample::renderMaterialSelection()
{
    if (mpScene->getMaterialCount() > 0)
    {
        mpGui->pushWindow("Select Material", 350, 100, 20, 300);

        // Generate materials list
        Gui::DropdownList materialDropdown;
        for (uint32_t i = 0; i < mpScene->getMaterialCount(); i++)
        {
            materialDropdown.push_back({(int32_t)i, mpScene->getMaterial(i)->getName()});
        }

        // Material selection
        mpGui->addDropdown("Materials", materialDropdown, mSelectedMaterialID);

        if (mpGui->addButton("Open"))
        {
            mpMaterial = mpScene->getMaterial(mSelectedMaterialID);
            mMaterialSelectionState = false;
        }

        mpGui->popWindow();
    }
    else
    {
        msgBox("Scene contains no materials!");
        mMaterialSelectionState = false;
    }
}

void MaterialEditorSample::loadModels()
{
    mDisplayModels[(uint32_t)DisplayModel::Sphere] = Model::createFromFile("sphere.obj", Model::GenerateTangentSpace);
    mDisplayModels[(uint32_t)DisplayModel::Cube]   = Model::createFromFile("box.obj", Model::GenerateTangentSpace);
    mDisplayModels[(uint32_t)DisplayModel::Teapot] = Model::createFromFile("teapot.obj", Model::GenerateTangentSpace);
}

void MaterialEditorSample::resetCamera()
{
    // update the camera position
    const auto& pModel = mDisplayModels[mActiveModel];
    const float radius = pModel->getRadius();
    const glm::vec3& modelCenter = pModel->getCenter();

    glm::vec3 cameraPos = modelCenter;
    cameraPos.z += radius * 2.0f;

    mpCamera->setPosition(cameraPos);
    mpCamera->setTarget(modelCenter);
    mpCamera->setUpVector(glm::vec3(0, 1, 0));

    // Update the controllers
    mCameraController.setModelParams(modelCenter, radius, 2.0f);
}

void MaterialEditorSample::onLoad()
{
    mpCamera = Camera::create();
    mpCamera->setDepthRange(0.01f, 1000.0f);

    mpProgram = GraphicsProgram::createFromFile("", "ModelViewer.ps.hlsl");

    // create rasterizer state
    RasterizerState::Desc solidDesc;
    solidDesc.setCullMode(RasterizerState::CullMode::Back);
    auto pRasterizerState = RasterizerState::create(solidDesc);

    // Depth test
    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthTest(true);
    auto pDepthState = DepthStencilState::create(dsDesc);

    mCameraController.attachCamera(mpCamera);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    auto pLinearSampler = Sampler::create(samplerDesc);

    mpDirLight = DirectionalLight::create();
    mpDirLight->setWorldDirection(glm::vec3(0.13f, 0.27f, -0.9f));

    mpPointLight = PointLight::create();

    mpProgramVars = GraphicsVars::create(mpProgram->getActiveVersion()->getReflector());
    mpGraphicsState = GraphicsState::create();

    mpGraphicsState->setProgram(mpProgram);
    mpGraphicsState->setRasterizerState(pRasterizerState);
    mpGraphicsState->setDepthStencilState(pDepthState);

    loadModels();
    resetCamera();
}

void MaterialEditorSample::onFrameRender()
{
    const glm::vec4 clearColor(0.38f, 0.52f, 0.10f, 1);
    mpRenderContext->clearFbo(mpDefaultFBO.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    mpGraphicsState->setFbo(mpDefaultFBO);

    mCameraController.update();

    if (mpMaterial)
    {
        mDisplayModels[mActiveModel]->getMesh(0)->setMaterial(mpMaterial);
    }

    mpDirLight->setIntoConstantBuffer(mpProgramVars["PerFrameCB"].get(), "gDirLight");
    mpPointLight->setIntoConstantBuffer(mpProgramVars["PerFrameCB"].get(), "gPointLight");

    mpProgramVars["PerFrameCB"]["gAmbient"] = mAmbientIntensity;

    mpRenderContext->setGraphicsState(mpGraphicsState);
    mpRenderContext->setGraphicsVars(mpProgramVars);

    ModelRenderer::render(mpRenderContext.get(), mDisplayModels[mActiveModel], mpCamera.get());
}

void MaterialEditorSample::onShutdown()
{

}

bool MaterialEditorSample::onKeyEvent(const KeyboardEvent& keyEvent)
{
    bool bHandled = mCameraController.onKeyEvent(keyEvent);
    if (bHandled == false)
    {
        if (keyEvent.type == KeyboardEvent::Type::KeyPressed)
        {
            switch (keyEvent.key)
            {
            case KeyboardEvent::Key::R:
                resetCamera();
                bHandled = true;
                break;
            }
        }
    }
    return bHandled;
}

bool MaterialEditorSample::onMouseEvent(const MouseEvent& mouseEvent)
{
    return mCameraController.onMouseEvent(mouseEvent);
}

void MaterialEditorSample::onDataReload()
{

}

void MaterialEditorSample::onResizeSwapChain()
{
    float height = (float)mpDefaultFBO->getHeight();
    float width = (float)mpDefaultFBO->getWidth();

    mpCamera->setFovY(float(M_PI / 3));
    float aspectRatio = (width / height);
    mpCamera->setAspectRatio(aspectRatio);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    MaterialEditorSample sample;
    SampleConfig config;
    config.windowDesc.title = "Material Editor";
    config.windowDesc.resizableWindow = true;
    sample.run(config);
}
