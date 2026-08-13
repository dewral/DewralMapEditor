#include "backend.h"

Backend *Backend::s_instance = nullptr;

Backend::Backend(QObject *parent)
    : QObject(parent), m_updateService(&m_docMgr), m_workTimer(&m_docMgr)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_otbReader.setDatReader(&m_datReader);
    m_otbReader.setItemsXml(&m_itemsXml);
    connect(&m_docMgr, &DocumentManager::currentChanged,
            this, &Backend::otbmReaderChanged);
}

Backend::~Backend()
{
    s_instance = nullptr;
}

Backend *Backend::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine)
    Q_ASSERT(s_instance);
    Q_ASSERT(engine->thread() == s_instance->thread());
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}
