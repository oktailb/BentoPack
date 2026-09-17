#ifndef __EXPORT_H__
#define __EXPORT_H__

#include <QString>
#include "packer/atlaspacker.h"

class Extractor;

enum Format {
    FORMAT_TEXTUREPACKER_JSON,
    FORMAT_SPARROW_XML,
    FORMAT_PHASER_JSON,
    FORMAT_CSS_SPRITES,
    FORMAT_UNITY,
    FORMAT_GODOT,
    FORMAT_ASEPRITE_JSON
};

enum AtlasStrategy {
    ATLASSTRATEGY_ORIGINAL_ATLAS,
    ATLASSTRATEGY_ONE_ATLAS_FOR_ALL_ANIMATIONS,
    ATLASSTRATEGY_ONE_ATLAS_PER_ANIMATION
};

struct ExportOptions {
    Format format = FORMAT_GODOT;
    bool trimSprites = false;
    bool rotateSprites = false;
    int padding = 2;
    bool compressJson = false;
    bool embedAnimations = true;
    QString namingConvention;

    AtlasPacker::PackOptions packOptions;
};

class ExportManager {
public:
    virtual bool exportFrames(const QString &basePath, const QString &projectName, Extractor* in) = 0;
};

#endif
