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
#include "Graphics/Material/MaterialEditor.h"
#include "Utils/Gui.h"
#include "Utils/OS.h"
#include "Graphics/TextureHelper.h"
#include "API/Texture.h"
#include "Graphics/Scene/Scene.h"
#include "Graphics/Scene/SceneExporter.h"

namespace Falcor
{
    // #TODO keep or remove?
    static const std::string kRemoveLayer  = "Remove Layer";
    static const std::string kAddLayer     = "Add Layer";
    static const std::string kActiveLayer  = "Active Layer";
    static const std::string kLayerType    = "Type";
    static const std::string kLayerNDF     = "NDF";
    static const std::string kLayerBlend   = "Blend";
    static const std::string kLayerGroup   = "Layer";
    static const std::string kAlbedo       = "Color";
    static const std::string kRoughness    = "Roughness";
    static const std::string kExtraParam   = "Extra Param";
    static const std::string kNormal       = "Normal";
    static const std::string kHeight       = "Height";
    static const std::string kAlpha        = "Alpha Test";
    static const std::string kAddTexture   = "Load Texture";
    static const std::string kClearTexture = "Clear Texture";

    Gui::DropdownList MaterialEditor::kLayerTypeDropdown =
    {
        { MatLambert,    "Lambert" },
        { MatConductor,  "Conductor" },
        { MatDielectric, "Dielectric" },
        { MatEmissive,   "Emissive" },
        { MatUser,       "Custom" }
    };

    Gui::DropdownList MaterialEditor::kLayerBlendDropdown =
    {
        { BlendFresnel,  "Fresnel" },
        { BlendAdd,      "Additive" },
        { BlendConstant, "Constant Factor" }
    };

    Gui::DropdownList MaterialEditor::kLayerNDFDropdown =
    {
        { NDFBeckmann, "Beckmann" },
        { NDFGGX,      "GGX" },
        { NDFUser,     "User Defined" }
    };

    Texture::SharedPtr loadTexture(bool useSrgb)
    {
        std::string filename;
        Texture::SharedPtr pTexture = nullptr;
        if(openFileDialog(nullptr, filename) == true)
        {
            pTexture = createTextureFromFile(filename, true, useSrgb);
            if(pTexture)
            {
                pTexture->setName(filename);
            }
        }
        return pTexture;
    }

    MaterialEditor::UniquePtr MaterialEditor::create(const Material::SharedPtr& pMaterial, bool useSrgb)
    {
        return UniquePtr(new MaterialEditor(pMaterial, useSrgb));
    }

    MaterialEditor::MaterialEditor(const Material::SharedPtr& pMaterial, bool useSrgb) : mpMaterial(pMaterial), mUseSrgb(useSrgb)
    {

        //// The height bias starts with 1 by default. Set it to zero
        //float zero = 0;
        //setHeightCB<0>(&zero, this);
        //initUI();
    }

#if 0
    void MaterialEditor::initUI()
    {
        mpGui = Gui::create("Material Editor");
        mpGui->setSize(300, 300);
        mpGui->setPosition(50, 300);

        mpGui->addButton("Save Material", &MaterialEditor::saveMaterial, this);
        mpGui->addSeparator();
        mpGui->addTextBoxWithCallback("Name", &MaterialEditor::setName, &MaterialEditor::getNameCB, this);
        mpGui->addIntVarWithCallback("ID", &MaterialEditor::setId, &MaterialEditor::getIdCB, this, "", 0);
        mpGui->addCheckBoxWithCallback("Double-Sided", &MaterialEditor::setDoubleSided, &MaterialEditor::getDoubleSidedCB, this);

        mpGui->addSeparator("");

        initNormalElements();
        initAlphaElements();
        initHeightElements();

        mpGui->addSeparator("");
        mpGui->addButton(kAddLayer, &MaterialEditor::addLayer, this);
        mpGui->addButton(kRemoveLayer, &MaterialEditor::removeLayerCB, this);
        mpGui->addSeparator("");
        mpGui->addIntVarWithCallback(kActiveLayer, &MaterialEditor::setActiveLayer, &MaterialEditor::getActiveLayerCB, this);

        refreshLayerElements();
    }

#endif

    void MaterialEditor::renderGui(Gui* pGui)
    {
        pGui->pushWindow("Material Editor", 400, 600, 20, 300);

        if (pGui->addButton("Save Material"))
        {
            saveMaterial();
        }

        pGui->addSeparator();

        setName(pGui);
        setId(pGui);
        setDoubleSided(pGui);
        pGui->addSeparator();

        setNormalMap(pGui);
        setAlphaMap(pGui);
        setHeightMap(pGui);

        setHeightModifiers(pGui);
        setAlphaThreshold(pGui);

        for (uint32_t i = 0; i < mpMaterial->getNumLayers(); i++)
        {
            std::string groupName("Layer " + std::to_string(i));

            if (pGui->beginGroup(groupName.c_str()))
            {
                setLayerTexture(pGui, i);
                setLayerType(pGui, i);
                setLayerNdf(pGui, i);
                setLayerBlend(pGui, i);

                const auto layer = mpMaterial->getLayer(i);

                switch (layer.type)
                {
                case Material::Layer::Type::Lambert:
                case Material::Layer::Type::Emissive:
                    setLayerAlbedo(pGui, i);
                    break;

                case Material::Layer::Type::Conductor:
                    setLayerAlbedo(pGui, i);
                    setLayerRoughness(pGui, i);
                    setConductorLayerParams(pGui, i);
                    break;

                case Material::Layer::Type::Dielectric:
                    setLayerAlbedo(pGui, i);
                    setLayerRoughness(pGui, i);
                    setDialectricLayerParams(pGui, i);
                    break;

                default:
                    break;
                }

                bool layerRemoved = removeLayer(pGui, i);

                pGui->endGroup();

                if (layerRemoved)
                {
                    break;
                }
            }
        }

        if (mpMaterial->getNumLayers() < MatMaxLayers)
        {
            pGui->addSeparator();
            addLayer(pGui);
        }

        pGui->popWindow();
    }

    Material* MaterialEditor::getMaterial(void* pUserData)
    {
        MaterialEditor* pEditor = (MaterialEditor*)pUserData;
        Material::SharedPtr pMaterial = pEditor->mpMaterial;
        return pMaterial.get();
    }

    void MaterialEditor::closeEditor()
    {
        implement me
    }

    void MaterialEditor::saveMaterial(Gui* pGui)
    {
        MaterialEditor* pEditor = (MaterialEditor*)pGui;
        pEditor->saveMaterial();
    }

    void MaterialEditor::setName(Gui* pGui)
    {
        char nameBuf[256];
        strcpy_s(nameBuf, mpMaterial->getName().c_str());

        if (pGui->addTextBox("Name", nameBuf, arraysize(nameBuf)))
        {
            mpMaterial->setName(nameBuf);
        }
    }

    void MaterialEditor::setId(Gui* pGui)
    {
        int32_t id = mpMaterial->getId();

        if (pGui->addIntVar("ID", id, 0))
        {
            mpMaterial->setID(id);
        }
    }

    void MaterialEditor::setDoubleSided(Gui* pGui)
    {
        bool doubleSided = mpMaterial->isDoubleSided();
        if (pGui->addCheckBox("Double Sided", doubleSided))
        {
            mpMaterial->setDoubleSided(doubleSided);
        }
    }

    void MaterialEditor::setHeightModifiers(Gui* pGui)
    {
        glm::vec2 heightMods = mpMaterial->getHeightModifiers();

        pGui->addFloatVar("Height Bias", heightMods[0], -FLT_MAX, FLT_MAX);
        pGui->addFloatVar("Height Scale", heightMods[1], 0.0f, FLT_MAX);

        mpMaterial->setHeightModifiers(heightMods);
    }

    void MaterialEditor::setAlphaThreshold(Gui* pGui)
    {
        float a = mpMaterial->getAlphaThreshold();

        if(pGui->addFloatVar("Alpha Threshold", a, 0.0f, 1.0f))
        {
            mpMaterial->setAlphaThreshold(a);
        }
    }

    void MaterialEditor::addLayer(Gui* pGui)
    {
        if (pGui->addButton("Add Layer"))
        {
            if (mpMaterial->getNumLayers() >= MatMaxLayers)
            {
                msgBox("Exceeded the number of supported layers. Can't add anymore");
                return;
            }

            mpMaterial->addLayer(Material::Layer());
        }
    }

    void MaterialEditor::setLayerType(Gui* pGui, uint32_t layerID)
    {
        uint32_t type = (uint32_t)mpMaterial->getLayer(layerID).type;

        std::string label("Type##" + std::to_string(layerID));
        if (pGui->addDropdown(label.c_str(), kLayerTypeDropdown, type))
        {
            mpMaterial->setLayerType(layerID, (Material::Layer::Type)type);
        }
    }

    void MaterialEditor::setLayerNdf(Gui* pGui, uint32_t layerID)
    {
        uint32_t ndf = (uint32_t)mpMaterial->getLayer(layerID).ndf;

        std::string label("NDF##" + std::to_string(layerID));
        if (pGui->addDropdown(label.c_str(), kLayerNDFDropdown, ndf))
        {
            mpMaterial->setLayerNdf(layerID, (Material::Layer::NDF)ndf);
        }
    }

    void MaterialEditor::setLayerBlend(Gui* pGui, uint32_t layerID)
    {
        uint32_t blend = (uint32_t)mpMaterial->getLayer(layerID).blend;

        std::string label("Blend##" + std::to_string(layerID));
        if (pGui->addDropdown(label.c_str(), kLayerBlendDropdown, blend))
        {
            mpMaterial->setLayerBlend(layerID, (Material::Layer::Blend)blend);
        }
    }

    void MaterialEditor::setLayerAlbedo(Gui* pGui, uint32_t layerID)
    {
        glm::vec4 albedo = mpMaterial->getLayer(layerID).albedo;

        std::string label("Albedo##" + std::to_string(layerID));
        if (pGui->addRgbaColor(label.c_str(), albedo))
        {
            mpMaterial->setLayerAlbedo(layerID, albedo);
        }
    }

    void MaterialEditor::setLayerRoughness(Gui* pGui, uint32_t layerID)
    {
        glm::vec4 roughness = mpMaterial->getLayer(layerID).roughness;

        std::string label("Roughness##" + std::to_string(layerID));
        if (pGui->addFloatVar(label.c_str(), roughness[0], 0.0f, 1.0f))
        {
            mpMaterial->setLayerRoughness(layerID, roughness);
        }
    }

    void MaterialEditor::setLayerTexture(Gui* pGui, uint32_t layerID)
    {
        const auto pTexture = mpMaterial->getLayer(layerID).pTexture;

        auto& pNewTexture = changeTexture(pGui, "Texture##" + std::to_string(layerID), pTexture);
        if (pNewTexture != nullptr)
        {
            mpMaterial->setLayerTexture(layerID, pNewTexture);
        }
    }

    void MaterialEditor::setConductorLayerParams(Gui* pGui, uint32_t layerID)
    {
        if (pGui->beginGroup("IoR"))
        {
            const auto layer = mpMaterial->getLayer(layerID);
            float r = layer.extraParam[0]; // #TODO Verify indices
            float i = layer.extraParam[1];

            pGui->addFloatVar("Real", r, 0.0f, FLT_MAX);
            pGui->addFloatVar("Imaginary", i, 0.0f, FLT_MAX);

            mpMaterial->setLayerUserParam(layerID, glm::vec4(r, i, 0.0f, 0.0f));

            pGui->endGroup();
        }
    }

    void MaterialEditor::setDialectricLayerParams(Gui* pGui, uint32_t layerID)
    {
        const auto layer = mpMaterial->getLayer(layerID);
        float ior = layer.extraParam[0];

        if (pGui->addFloatVar("IoR", ior, 0.0f, FLT_MAX))
        {
            mpMaterial->setLayerUserParam(layerID, glm::vec4(ior, 0.0f, 0.0f, 0.0f));
        }
    }

    bool MaterialEditor::removeLayer(Gui* pGui, uint32_t layerID)
    {
        std::string label("Remove##" + std::to_string(layerID));
        if (pGui->addButton(label.c_str()))
        {
            mpMaterial->removeLayer(layerID);
            return true;
        }

        return false;
    }

    void MaterialEditor::setNormalMap(Gui* pGui)
    {
        const auto pTexture = mpMaterial->getNormalMap();

        auto& pNewTexture = changeTexture(pGui, "Normal Map", pTexture);
        if (pNewTexture != nullptr)
        {
            mpMaterial->setNormalMap(pNewTexture);
        }
    }

    void MaterialEditor::setAlphaMap(Gui* pGui)
    {
        const auto pTexture = mpMaterial->getAlphaMap();

        auto& pNewTexture = changeTexture(pGui, "Alpha Map", pTexture);
        if (pNewTexture != nullptr)
        {
            mpMaterial->setAlphaMap(pNewTexture);
        }
    }

    void MaterialEditor::setHeightMap(Gui* pGui)
    {
        const auto pTexture = mpMaterial->getHeightMap();

        auto& pNewTexture = changeTexture(pGui, "Height Map", pTexture);
        if (pNewTexture != nullptr)
        {
            mpMaterial->setHeightMap(pNewTexture);
        }
    }

    Texture::SharedPtr MaterialEditor::changeTexture(Gui* pGui, const std::string& label, const Texture::SharedPtr& pCurrentTexture)
    {
        std::string texPath(pCurrentTexture ? pCurrentTexture->getSourceFilename() : "");

        char texPathBuff[1024];
        strcpy_s(texPathBuff, texPath.c_str());

        pGui->addTextBox(label.c_str(), texPathBuff, arraysize(texPathBuff));

        std::string buttonLabel("Change##" + label);
        if (pGui->addButton(buttonLabel.c_str(), true))
        {
            return loadTexture(mUseSrgb);
        }

        return nullptr;
    }

#if 0
    void MaterialEditor::initAlbedoElements() const
    {
        load_textures(Albedo);
        add_value_var(Albedo, "r", 0, 0, FLT_MAX, "Base Color");
        add_value_var(Albedo, "g", 1, 0, FLT_MAX, "Base Color");
        add_value_var(Albedo, "b", 2, 0, FLT_MAX, "Base Color");
        mpGui->nestGroups(kAlbedo, "Base Color");
        mpGui->nestGroups(kLayerGroup, kAlbedo);
    }

    void MaterialEditor::initRoughnessElements() const
    {
        load_textures(Roughness);
        add_value_var(Roughness, "Base Roughness", 0, 0, FLT_MAX, kRoughness);
        mpGui->nestGroups(kLayerGroup, kRoughness);
    }

    void MaterialEditor::initNormalElements() const
    {
        load_textures(Normal);
    }

    void MaterialEditor::initAlphaElements() const
    {
        load_textures(Alpha);
        add_value_var(Alpha, "Alpha Threshold", 0, 0, 1, kAlpha);
    }

    void MaterialEditor::initHeightElements() const
    {
        load_textures(Height);
        add_value_var(Height, "Height Bias", 0, -FLT_MAX, FLT_MAX, kHeight);
        add_value_var(Height, "Height Scale", 1, 0, FLT_MAX, kHeight);
    }

    void MaterialEditor::initLambertLayer() const
    {
        initAlbedoElements();
    }

    void MaterialEditor::initConductorLayer() const
    {
        initAlbedoElements();
        initRoughnessElements();
        add_value_var(ExtraParam, "Real Part", 0, 0, FLT_MAX, "IoR");
        add_value_var(ExtraParam, "Imaginery Part", 1, 0, FLT_MAX, "IoR");
        mpGui->nestGroups(kLayerGroup, "IoR");
        initLayerNdfElements();
    }

    void MaterialEditor::initDielectricLayer() const
    {
        initAlbedoElements();
        initRoughnessElements();
        add_value_var(ExtraParam, "IoR", 0, 0, FLT_MAX, kLayerGroup);
    }

    void MaterialEditor::initEmissiveLayer() const
    {
        initAlbedoElements();
    }
#endif

    void MaterialEditor::saveMaterial()
    {
        std::string filename;
        if(saveFileDialog("Scene files\0*.fscene\0\0", filename))
        {
            // Using the scene exporter
            Scene::SharedPtr pScene = Scene::create();
            pScene->addMaterial(mpMaterial);

            SceneExporter::saveScene(filename, pScene.get(), SceneExporter::ExportMaterials);
        }
    }
}
