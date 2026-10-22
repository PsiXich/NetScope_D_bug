#pragma once
#include "core/ConnectionManager.h"
#include <QWidget>
class QLineEdit; class QSpinBox; class QPushButton; class QLabel;
class QComboBox; class QCheckBox; class QPlainTextEdit; class QListWidget;
class MqttClientPanel : public QWidget {
    Q_OBJECT
public:
    explicit MqttClientPanel(int id, ConnectionManager *manager, QWidget *parent = nullptr);
private:
    void refresh();
    int m_id;
    ConnectionManager *m_manager;
    QLineEdit *m_host, *m_clientId, *m_username, *m_password, *m_filter, *m_topic;
    QSpinBox *m_port;
    QPushButton *m_connect, *m_disconnect, *m_subscribe, *m_unsubscribe, *m_publish;
    QLabel *m_status, *m_feedback;
    QComboBox *m_subQos, *m_pubQos;
    QCheckBox *m_retain, *m_hex;
    QPlainTextEdit *m_payload;
    QListWidget *m_subscriptions;
};
