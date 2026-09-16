#include "arrangementmodel.h"
#include "model/spritedocument.h"
#include <QDebug>
#include <QIODevice>
#include <QPixmap>

ArrangementModel::ArrangementModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

void ArrangementModel::setDocument(SpriteDocument *doc)
{
    if (m_document == doc) return;
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }
    m_document = doc;
    clearThumbnailCache();

    if (m_document) {
        connect(m_document, &SpriteDocument::framesChanged, this, &ArrangementModel::clearThumbnailCache);
        connect(m_document, &SpriteDocument::documentReset, this, &ArrangementModel::clearThumbnailCache);
        connect(m_document, &SpriteDocument::frameUpdated, this, [this](int idx) {
            m_thumbnailCache.remove(idx);
            QModelIndex mIdx = index(idx, 0);
            if (mIdx.isValid()) {
                emit dataChanged(mIdx, mIdx, {Qt::DecorationRole});
            }
        });
    }
}

void ArrangementModel::clearThumbnailCache()
{
    m_thumbnailCache.clear();
}

QVariant ArrangementModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DecorationRole && index.isValid()) {
        int row = index.row();
        auto it = m_thumbnailCache.constFind(row);
        if (it != m_thumbnailCache.constEnd()) {
            return it.value();
        }
        if (m_document && row >= 0 && row < m_document->frameCount()) {
            const QImage &img = m_document->frame(row);
            if (!img.isNull()) {
                QPixmap pm = QPixmap::fromImage(img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                m_thumbnailCache.insert(row, pm);
                return pm;
            }
        }
    }
    return QStandardItemModel::data(index, role);
}

Qt::ItemFlags ArrangementModel::flags(const QModelIndex &index) const
{
  // Retrieve the default flags for the item
  Qt::ItemFlags defaultFlags = QStandardItemModel::flags(index);

  if (index.isValid()) {
      // For a valid item, enable dragging (source) and dropping (target for merge)
      return defaultFlags | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }

  // If the index is invalid (dropping into empty space/list end), still allow dropping
  // for standard reordering functionality (handled by the base class).
  return defaultFlags | Qt::ItemIsDropEnabled;
}

QMimeData *ArrangementModel::mimeData(const QModelIndexList &indexes) const
{
  // Use the default implementation for standard data
  QMimeData *mimeData = QStandardItemModel::mimeData(indexes);

         // Add custom data to easily identify the source row
  if (!indexes.isEmpty()) {
      QByteArray encodedData;
      QDataStream stream(&encodedData, QIODevice::WriteOnly);

      // We only care about the first selected row for the drag source
      stream << indexes.first().row();

      // Custom mime type used to transport the source row index
      mimeData->setData("application/x-sprite-row", encodedData);
    }
  return mimeData;
}

bool ArrangementModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                int row, int column, const QModelIndex &parent)
{
  // 1. Check for MERGE operation (Drop ONTO an existing item)
  // 'parent.isValid()' is true when an item is dropped directly onto another item.
  if (parent.isValid() && data->hasFormat("application/x-sprite-row")) {

      // Extract the source row from the custom mime data
      QByteArray encodedData = data->data("application/x-sprite-row");
      QDataStream stream(&encodedData, QIODevice::ReadOnly);
      int sourceRow;
      stream >> sourceRow;

      int targetRow = parent.row();

      // Prevent dropping an item onto itself
      if (sourceRow != targetRow) {
          // Emit a signal to MainWindow to execute the heavy-lifting merge logic
          emit mergeRequested(sourceRow, targetRow);
          return true; // Drop successfully accepted and handled (no standard reordering needed)
        }
    }

  // 2. Otherwise, perform STANDARD REORDERING (Drop BETWEEN items or into empty space)
  return QStandardItemModel::dropMimeData(data, action, row, column, parent);
}
