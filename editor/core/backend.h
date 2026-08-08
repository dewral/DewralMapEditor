#ifndef BACKEND_H
#define BACKEND_H

#include <QObject>
#include <QQmlEngine>
#include <QJSEngine>
#include <QtQml/qqmlregistration.h>

#include "sprreader.h"
#include "datreader.h"
#include "otbreader.h"
#include "otbmreader.h"
#include "otfireader.h"
#include "itemsxmlreader.h"
#include "documentmanager.h"
#include "filetools.h"
#include "tilesetstore.h"
#include "brushstore.h"
#include "uitheme.h"
#include "creaturestore.h"
#include "aimapassistant.h"

class Backend : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Backend)
    QML_SINGLETON
    Q_PROPERTY(SprReader *sprReader READ sprReader CONSTANT)
    Q_PROPERTY(DatReader *datReader READ datReader CONSTANT)
    Q_PROPERTY(OtbReader *otbReader READ otbReader CONSTANT)
    Q_PROPERTY(DocumentManager *docMgr READ docMgr CONSTANT)
    Q_PROPERTY(OtbmReader *otbmReader READ otbmReader NOTIFY otbmReaderChanged)
    Q_PROPERTY(FileTools *fileTools READ fileTools CONSTANT)
    Q_PROPERTY(TilesetStore *tilesetStore READ tilesetStore CONSTANT)
    Q_PROPERTY(BrushStore *brushStore READ brushStore CONSTANT)
    Q_PROPERTY(OtfiReader *otfiReader READ otfiReader CONSTANT)
    Q_PROPERTY(UiTheme *uiTheme READ uiTheme CONSTANT)
    Q_PROPERTY(CreatureStore *creatureStore READ creatureStore CONSTANT)
    Q_PROPERTY(ItemsXmlReader *itemsXml READ itemsXml CONSTANT)
    Q_PROPERTY(AiMapAssistant *aiMapAssistant READ aiMapAssistant CONSTANT)

public:
    // An explicit parent makes QML use create() instead of constructing a second singleton.
    explicit Backend(QObject *parent);
    ~Backend() override;
    static Backend *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    SprReader *sprReader() { return &m_sprReader; }
    DatReader *datReader() { return &m_datReader; }
    OtbReader *otbReader() { return &m_otbReader; }
    DocumentManager *docMgr() { return &m_docMgr; }
    OtbmReader *otbmReader() { return m_docMgr.current(); }
    FileTools *fileTools() { return &m_fileTools; }
    TilesetStore *tilesetStore() { return &m_tilesetStore; }
    BrushStore *brushStore() { return &m_brushStore; }
    OtfiReader *otfiReader() { return &m_otfiReader; }
    UiTheme *uiTheme() { return &m_uiTheme; }
    CreatureStore *creatureStore() { return &m_creatureStore; }
    ItemsXmlReader *itemsXml() { return &m_itemsXml; }
    AiMapAssistant *aiMapAssistant() { return &m_aiMapAssistant; }

    Q_INVOKABLE int preloadPaletteSprites() {
        return m_sprReader.preloadItemImageSources(&m_datReader);
    }

signals:
    void otbmReaderChanged();

private:
    static Backend *s_instance;
    SprReader m_sprReader;
    DatReader m_datReader;
    ItemsXmlReader m_itemsXml;
    OtbReader m_otbReader;
    DocumentManager m_docMgr;
    FileTools m_fileTools;
    TilesetStore m_tilesetStore;
    BrushStore m_brushStore;
    CreatureStore m_creatureStore;
    OtfiReader m_otfiReader;
    UiTheme m_uiTheme;
    AiMapAssistant m_aiMapAssistant;
};

#endif
