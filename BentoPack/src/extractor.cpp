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

#include "extractor/extractor.h"
#include <QFileInfo>

Extractor::Extractor(QObject *parent)
    : QObject(parent)
{
}

void Extractor::setProgress(int percentage)
{
    m_progress = percentage;
    emit progress(percentage);
}

void Extractor::setStatusMessage(const QString &message)
{
    m_statusMessage = message;
    emit statusMessage(message);
}

bool Extractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    return supportedExtensions().contains(fi.suffix().toLower());
}

bool Extractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(inDoc);
    Q_UNUSED(options);
    if (error) {
        error->code = ExtractorError::UnsupportedFormat;
        error->message = tr("Export is not supported by this format (%1)").arg(displayName());
        error->filePath = filePath;
    }
    return false;
}

bool Extractor::extract(const QString &filePath, SpriteDocument &doc, QString *errorMsg)
{
    ExtractorError err;
    bool ok = read(filePath, doc, &err);
    if (!ok && errorMsg) {
        *errorMsg = err.toString();
    }
    return ok;
}

bool Extractor::exportDocument(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, QString *errorMsg)
{
    ExtractorError err;
    bool ok = write(filePath, doc, options, &err);
    if (!ok && errorMsg) {
        *errorMsg = err.toString();
    }
    return ok;
}
