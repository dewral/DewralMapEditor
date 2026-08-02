#include "aimapassistant.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QUrl>

namespace {

QString responseText(const QJsonObject &response)
{
    const QJsonArray output = response.value(QStringLiteral("output")).toArray();
    for (const QJsonValue &itemValue : output) {
        const QJsonArray content = itemValue.toObject().value(QStringLiteral("content")).toArray();
        for (const QJsonValue &contentValue : content) {
            const QJsonObject contentItem = contentValue.toObject();
            if (contentItem.value(QStringLiteral("type")).toString() == QLatin1String("output_text"))
                return contentItem.value(QStringLiteral("text")).toString();
        }
    }
    return {};
}

QJsonObject operationSchema()
{
    return {
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("additionalProperties"), false},
        {QStringLiteral("properties"), QJsonObject{
             {QStringLiteral("x"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}},
             {QStringLiteral("y"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}},
             {QStringLiteral("brush"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
         }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("brush")}}
    };
}

} // namespace

AiMapAssistant::AiMapAssistant(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this))
{
}

bool AiMapAssistant::configured() const
{
    return !qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty();
}

QString AiMapAssistant::model() const
{
    const QString configuredModel = qEnvironmentVariable("OPENAI_MODEL").trimmed();
    return configuredModel.isEmpty() ? QStringLiteral("gpt-5.6-luna") : configuredModel;
}

void AiMapAssistant::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged();
}

void AiMapAssistant::cancel()
{
    if (m_reply) m_reply->abort();
}

void AiMapAssistant::generate(const QString &prompt, const QVariantMap &selection)
{
    if (m_busy) return;
    if (!configured()) {
        emit failed(QStringLiteral("Set OPENAI_API_KEY before starting DME."));
        return;
    }
    if (prompt.trimmed().isEmpty()) {
        emit failed(QStringLiteral("Describe what should be generated."));
        return;
    }
    if (!selection.value(QStringLiteral("valid")).toBool()) {
        emit failed(selection.value(QStringLiteral("error"),
                                    QStringLiteral("Select a map area first.")).toString());
        return;
    }

    const QJsonObject planSchema{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("additionalProperties"), false},
        {QStringLiteral("properties"), QJsonObject{
             {QStringLiteral("summary"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
             {QStringLiteral("operations"), QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("array")},
                  {QStringLiteral("maxItems"), 1024},
                  {QStringLiteral("items"), operationSchema()}
              }}
         }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("summary"), QStringLiteral("operations")}}
    };

    const QString instructions = QStringLiteral(
        "You are the terrain planner for Dewral Map Editor. Return only a plan matching the schema. "
        "Each operation paints one selected tile using one exact ground brush name from available_brushes. "
        "Coordinates are relative to the selection origin. Never use coordinates absent from selected_tiles. "
        "Preserve existing terrain unless changing it helps satisfy the request. Do not invent brush names. "
        "Produce a coherent top-down OpenTibia map fragment, not random noise.");

    QJsonObject body;
    body.insert(QStringLiteral("model"), model());
    body.insert(QStringLiteral("store"), false);
    body.insert(QStringLiteral("instructions"), instructions);
    body.insert(QStringLiteral("input"),
                prompt.trimmed() + QStringLiteral("\n\nMAP_CONTEXT:\n")
                    + QString::fromUtf8(QJsonDocument::fromVariant(selection).toJson(QJsonDocument::Compact)));
    body.insert(QStringLiteral("text"), QJsonObject{
        {QStringLiteral("format"), QJsonObject{
             {QStringLiteral("type"), QStringLiteral("json_schema")},
             {QStringLiteral("name"), QStringLiteral("dme_map_plan")},
             {QStringLiteral("strict"), true},
             {QStringLiteral("schema"), planSchema}
         }}
    });

    QNetworkRequest request(QUrl(QStringLiteral("https://api.openai.com/v1/responses")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + qEnvironmentVariable("OPENAI_API_KEY").toUtf8());
    m_reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, &AiMapAssistant::finishReply);
    setBusy(true);
}

void AiMapAssistant::finishReply()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    setBusy(false);
    if (!reply) return;

    const QByteArray payload = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkMessage = reply->errorString();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument responseDoc = QJsonDocument::fromJson(payload, &parseError);
    const QJsonObject response = responseDoc.object();
    if (networkError != QNetworkReply::NoError) {
        const QString apiMessage = response.value(QStringLiteral("error")).toObject()
                                       .value(QStringLiteral("message")).toString();
        emit failed(apiMessage.isEmpty() ? networkMessage : apiMessage);
        return;
    }
    if (parseError.error != QJsonParseError::NoError) {
        emit failed(QStringLiteral("The AI service returned invalid JSON: %1").arg(parseError.errorString()));
        return;
    }

    const QString text = responseText(response);
    const QJsonDocument planDoc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !planDoc.isObject()) {
        emit failed(QStringLiteral("The AI response did not contain a valid map plan."));
        return;
    }
    emit planReady(planDoc.object().toVariantMap());
}
