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
#include "MultiPassPostProcess.h"

void MultiPassPostProcess::onGuiRender()
{
    if (mpGui->addButton("Load Image"))
    {
        loadImage();
    }
    mpGui->addCheckBox("Radial Blur", mEnableRadialBlur);
    if(mEnableRadialBlur)
    {
        mpGui->addCheckBox("Grayscale", mEnableGrayscale);
    }
}

void MultiPassPostProcess::onLoad()
{
    mpLuminance = FullScreenPass::create("Luminance.fs");
    mpRadialBlur = FullScreenPass::create("RadialBlur.fs");
    mpBlit = FullScreenPass::create("Blit.fs");

    init_tests();

    std::vector<ArgList::Arg> filenames = mArgList.getValues("loadimage");
    if (!filenames.empty())
    {
        loadImageFromFile(filenames[0].asString());
    }

    if (mArgList.argExists("radialblur"))
    {
        mEnableRadialBlur = true;
        if (mArgList.argExists("grayscale"))
        {
            mEnableGrayscale = true;
        }
    }
}

void MultiPassPostProcess::loadImage()
{
    std::string filename;
    if(openFileDialog("Supported Formats\0*.jpg;*.bmp;*.dds;*.png;*.tiff;*.tif;*.tga\0\0", filename))
    {
        loadImageFromFile(filename);
    }
}

void MultiPassPostProcess::loadImageFromFile(std::string filename)
{
    auto fboFormat = mpDefaultFBO->getColorTexture(0)->getFormat();
    mpImage = createTextureFromFile(filename, false, isSrgbFormat(fboFormat));
    ResourceFormat imageFormat = mpImage->getFormat();
    Fbo::Desc fboDesc;
    fboDesc.setColorTarget(0, mpImage->getFormat());
    mpTempFB = FboHelper::create2D(mpImage->getWidth(), mpImage->getHeight(), fboDesc);

    resizeSwapChain(mpImage->getWidth(), mpImage->getHeight());
    mpProgVars[0] = GraphicsVars::create(mpBlit->getProgram()->getActiveVersion()->getReflector());
    mpProgVars[0]->setTexture("gTexture", mpImage);

    mpProgVars[1] = GraphicsVars::create(mpLuminance->getProgram()->getActiveVersion()->getReflector());
    mpProgVars[1]->setTexture("gTexture", mpTempFB->getColorTexture(0));
}

void MultiPassPostProcess::onFrameRender()
{
    const glm::vec4 clearColor(0.38f, 0.52f, 0.10f, 1);
    mpRenderContext->clearFbo(mpDefaultFBO.get(), clearColor, 0, 0, FboAttachmentType::Color);

    if(mpImage)
    {
        // Grayscale is only with radial blur
        mEnableGrayscale = mEnableRadialBlur && mEnableGrayscale;

        mpRenderContext->setGraphicsVars(mpProgVars[0]);

        if(mEnableRadialBlur)
        {
            mpRenderContext->getGraphicsState()->pushFbo(mpTempFB);
            mpRadialBlur->execute(mpRenderContext.get());
            mpRenderContext->getGraphicsState()->popFbo();

            mpRenderContext->setGraphicsVars(mpProgVars[1]);
            const FullScreenPass* pFinalPass = mEnableGrayscale ? mpLuminance.get() : mpBlit.get();
            pFinalPass->execute(mpRenderContext.get());
        }
        else
        {
            mpBlit->execute(mpRenderContext.get());
        }
    }

    run_test();
}

void MultiPassPostProcess::onShutdown()
{
}

bool MultiPassPostProcess::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        switch (keyEvent.key)
        {
        case KeyboardEvent::Key::L:
            loadImage();
            return true;
        case KeyboardEvent::Key::G:
            mEnableGrayscale = true;
            return true;
        case KeyboardEvent::Key::R:
            mEnableRadialBlur = true;
            return true;
        }
    }
    return false;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    MultiPassPostProcess multiPassPostProcess;
    SampleConfig config;
    config.windowDesc.title = "Multi-pass post-processing";
    multiPassPostProcess.run(config);
}
