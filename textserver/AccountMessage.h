#pragma once

#include <QVariant>

#include "Message.h"
#include "User.h"

class AccountMessage :
	public Message
{
private:
	User user;
	quint16 fieldType;
	QVariant field;

public:
	AccountMessage(MessageType m, QDataStream& streamIn);
	~AccountMessage();

	QString getUserName();
	QString getNickname();
	QString getPasswd();
	QImage getIcon();
	quint16 getFieldType();
	QVariant getField();
};

enum AccountFieldType 
{
	ChangeNickname,
	ChangeIcon,
	ChangePassword,
	WrongFieldType
};