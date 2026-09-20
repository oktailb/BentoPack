#include "sample_extractor.h"

bool SampleExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    Q_UNUSED(filePath);
    Q_UNUSED(outDoc);
    if (error) {
        error->code = ExtractorError::NoError;
    }
    return true;
}

bool SampleExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(filePath);
    Q_UNUSED(inDoc);
    Q_UNUSED(options);
    if (error) {
        error->code = ExtractorError::NoError;
    }
    return true;
}
