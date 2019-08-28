#ifndef LIVETEXT_H
#define LIVETEXT_H

#include "textedit.h"
#include "LandingPage.h"
#include "Client.h"
#include <User.h>

#include <QObject>

class LiveText : public QObject
{
public:
	LiveText(QObject* parent = nullptr);
	~LiveText();

	void start();


private slots:
	//LANDING PAGE
	void openEditor(int mode, QString path);
	void connectToServer(QString ipAddress, quint16 port);
	void Login(QString username, QString password);
	void Register(QString username, QString password, QString nickname, QImage icon);
	void Logout();
	
	//CLIENT



	//TEXTEDIT
	void returnToLanding();
	void sendCursor(qint32 pos);
	void sendAccountUpdate(QString name, QImage image);

	//LIVETEXT
	void loginFailed(QString errorType);
	void registrationFailed(QString errorType);
	void loginSuccess(User user);
	void registrationSuccess(User user);

private:
	LandingPage* _landingPage;
	Client* _client;
	TextEdit* _textEdit;
	User _user;
};



#endif //LIVETEXT_H