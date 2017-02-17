/***************************************************************************
# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
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
#include "Graphics/Paths/PathRenderer.h"
#include "Data/VertexAttrib.h"
#include "API/RenderContext.h"
#include "Graphics/Camera/Camera.h"

namespace Falcor
{
    PathRenderer::UniquePtr PathRenderer::create(uint32_t maxVertices)
    {
        return UniquePtr(new PathRenderer(maxVertices));
    }

    PathRenderer::PathRenderer(uint32_t maxVertices)
    {
        mpVertexBuffer = Buffer::create(sizeof(glm::vec3) * maxVertices, Resource::BindFlags::Vertex, Buffer::CpuAccess::Write, nullptr);

        VertexBufferLayout::SharedPtr pBufferLayout = VertexBufferLayout::create();
        pBufferLayout->addElement(VERTEX_POSITION_NAME, VERTEX_POSITION_LOC, ResourceFormat::RGB32Float, 1, 0);

        VertexLayout::SharedPtr pLayout = VertexLayout::create();
        pLayout->addBufferLayout(0, pBufferLayout);

        mpVao = Vao::create({ mpVertexBuffer }, pLayout, nullptr, ResourceFormat::Unknown, Vao::Topology::LineList);

        mVertexData.resize(maxVertices);
    }

    void PathRenderer::setCameraData(RenderContext* pContext, Camera* pCamera)
    {
        //#TODO Do this properly
        const auto& bufferDesc = pContext->getGraphicsVars()->getReflection()->getBufferDesc("InternalPerFrameCB", ProgramReflection::BufferReflection::Type::Constant);
        size_t camDataOffset = bufferDesc->getVariableData("gCam.viewMat")->location;

        ConstantBuffer* pCB = pContext->getGraphicsVars()->getConstantBuffer("InternalPerFrameCB").get();
        pCamera->setIntoConstantBuffer(pCB, camDataOffset);
    }

    void PathRenderer::renderPath(const ObjectPath::SharedPtr& pPath, RenderContext* pContext, Camera* pCamera)
    {
        setCameraData(pContext, pCamera);

        mVertexData.clear();

        // Line segments connecting each keyframe
        const uint32_t detail = 10;

        uint32_t vertexCount = 0;
        for (uint32_t frameID = 0; frameID < pPath->getKeyFrameCount() - 1; frameID++)
        {
            ObjectPath::Frame lastFrame = pPath->getFrameAt(frameID, 0.0f);

            for (uint_t i = 1; i <= detail; i++)
            {
                float t = (float)i / (float)detail;
                ObjectPath::Frame currFrame = pPath->getFrameAt(frameID, t);

                mVertexData.push_back(lastFrame.position);
                mVertexData.push_back(currFrame.position);
                vertexCount += 2;

                lastFrame = currFrame;

                //#TODO bounds check vertex push
            }
        }

        mpVertexBuffer->updateData(mVertexData.data(), 0, sizeof(glm::vec3) * vertexCount);
        pContext->getGraphicsState()->setVao(mpVao);
        pContext->draw(vertexCount, 0);

    }

}
