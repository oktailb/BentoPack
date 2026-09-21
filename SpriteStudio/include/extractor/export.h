/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#ifndef __EXPORT_H__
#define __EXPORT_H__

#include <QString>
#include "packer/atlaspacker.h"
#include "packer/vramtexturecompressor.h"
#include "spritestudiocore_export.h"

class Extractor;

enum Format {
    FORMAT_TEXTUREPACKER_JSON,
    FORMAT_SPARROW_XML,
    FORMAT_PHASER_JSON,
    FORMAT_CSS_SPRITES,
    FORMAT_UNITY,
    FORMAT_GODOT,
    FORMAT_ASEPRITE_JSON,
    FORMAT_UNREAL
};

enum TextureFormat {
    TEXTURE_FORMAT_PNG,
    TEXTURE_FORMAT_KTX2_UASTC,
    TEXTURE_FORMAT_KTX2_ETC1S,
    TEXTURE_FORMAT_BASIS
};

enum AtlasStrategy {
    ATLASSTRATEGY_ORIGINAL_ATLAS,
    ATLASSTRATEGY_ONE_ATLAS_FOR_ALL_ANIMATIONS,
    ATLASSTRATEGY_ONE_ATLAS_PER_ANIMATION
};

#include <QVariantMap>

struct SPRITESTUDIO_CORE_EXPORT ExportOptions {
    Format format = FORMAT_GODOT;
    QString formatId; // Dynamic format ID (e.g. "godot", "unity", "unreal", "json")
    TextureFormat textureFormat = TEXTURE_FORMAT_PNG;
    VramCompressionOptions vramOptions;

    bool trimSprites = false;
    bool rotateSprites = false;
    int padding = 2;
    bool compressJson = false;
    bool embedAnimations = true;
    QString namingConvention;

    AtlasPacker::PackOptions packOptions;
    QVariantMap extraParams; // Extensible parameters passed directly to dynamic plugins
};

class ExportManager {
public:
    virtual bool exportFrames(const QString &basePath, const QString &projectName, Extractor* in) = 0;
};

#endif
