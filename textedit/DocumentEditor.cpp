#include "DocumentEditor.h"

#include <QDebug>

DocumentEditor::DocumentEditor(Document doc, TextEdit* editor, User& user, QObject* parent)
	: QObject(parent), _document(doc), _textedit(editor), _user(user)
{
	qRegisterMetaType<TextBlockID>("TextBlockID");
	qRegisterMetaType<TextBlock>("TextBlock");
	qRegisterMetaType<TextListID>("TextListID");
	qRegisterMetaType<TextList>("TextList");
	qRegisterMetaType<Symbol>("Symbol");
	qRegisterMetaType<QTextBlockFormat>("QTextBlockFormat");
	qRegisterMetaType<QTextCharFormat>("QTextCharFormat");
	qRegisterMetaType<QTextListFormat>("QTextListFormat");
	qRegisterMetaType<QVector<qint32>>("QVector<qint32>");
}

void DocumentEditor::openDocument()
{
	for (int i = 0; i < _document.length() - 1; i++) {
		_textedit->newChar(_document[i].getChar(), _document[i].getFormat(), i);
	}
	   
	foreach(TextList list, _document._lists.values()) 
	{
		QList<TextBlockID> blocks = list.getBlocks();
		int firstListBlockPosition = _document.getBlockPosition(blocks.first());

		_textedit->createList(firstListBlockPosition, list.getFormat());
		blocks.removeFirst();

		foreach(TextBlockID id, blocks) {
			_textedit->addBlockToList(_document.getBlockPosition(id), firstListBlockPosition);
		}
	}

	foreach(TextBlock block, _document._blocks.values()) {
		_textedit->applyBlockFormat(_document.getBlockPosition(block.getId()), block.getFormat());
	}


	_textedit->setCurrentFileName(_document.getName(), _document.getURI().toString());
	_textedit->startCursorTimer();
}


//From Server to Client
void DocumentEditor::addSymbol(Symbol s, bool isLast)
{
	qDebug().nospace() << "Remote char insertion: " << s.getChar();
	int position = _document.insert(s);
	if (!isLast) {	// skip inserting this char in the Qt document, because it inserts one automatically
		_textedit->newChar(s.getChar(), s.getFormat(), position);
		_textedit->updateUsersSelections();
	}
}

void DocumentEditor::removeSymbol(QVector<int> position)
{
	qDebug().nospace() << "Remote char deletion: " << _document[position].getChar();
	int pos = _document.remove(position);
	_textedit->removeChar(pos);
	_textedit->updateUsersSelections();
}

//From Client to Server
void DocumentEditor::deleteCharAtIndex(int position)
{
	if (position >= _document.length()) {
		qDebug() << "Skipped deletion of out-of-range symbol, pos =" << position;
		return;
	}

	qDebug().nospace() << "Local char deletion: " << _document[position].getChar();
	QVector<qint32> fractionalPosition = _document.removeAtIndex(position);
	emit deleteChar(fractionalPosition);
}

void DocumentEditor::addCharAtIndex(QChar ch, QTextCharFormat fmt, int position, bool isLast)
{
	Symbol s(ch, fmt, _user.getUserId(), _document.newFractionalPos(position));

	qDebug().nospace() << "Local char insertion: " << s.getChar();

	_document.insert(s);
	emit insertChar(s, isLast);
}


//Generating extra selections for user
void DocumentEditor::generateExtraSelection()
{
	if (_document.length() > 0) {
		QPair<int, int> selectionDelimiters;

		//Get firs character author
		qint32 userId = _document[0].getAuthorId();
		selectionDelimiters.first = 0;
		selectionDelimiters.second = 0;

		//Increment selection upper limit until we reach end of document or a char inserted by another user
		for (int i = 0; i < _document.length() - 1; i++) {
			if (_document[i].getAuthorId() != userId) {
				_textedit->setExtraSelections(userId, selectionDelimiters);

				userId = _document[i].getAuthorId();
				selectionDelimiters.first = i;
				selectionDelimiters.second = i;
			}
			selectionDelimiters.second++;
		}

		_textedit->setExtraSelections(userId, selectionDelimiters);
	}
}

//Block format
void DocumentEditor::changeBlockAlignment(int start, int end, Qt::Alignment alignment)
{
	QList<TextBlockID> blocks = _document.getBlocksBetween(start, end);

	foreach(TextBlockID textBlock, blocks) 
	{
		QTextBlockFormat fmt = _document.getBlock(textBlock).getFormat();
		fmt.setAlignment(alignment);

		qDebug().nospace() << "Local alignment change of block {" << textBlock.getBlockNumber()
			<< ", " << textBlock.getAuthorId() << "}";

		_document.formatBlock(textBlock, fmt);
		emit blockFormatChanged(textBlock, fmt);
	}
}

void DocumentEditor::changeBlockLineHeight(int start, int end, qreal height, int heightType)
{
	QList<TextBlockID> blocks = _document.getBlocksBetween(start, end);

	foreach(TextBlockID textBlock, blocks) 
	{
		QTextBlockFormat fmt = _document.getBlock(textBlock).getFormat();
		fmt.setLineHeight(height, heightType);

		qDebug().nospace() << "Local lineHeight change of block {" << textBlock.getBlockNumber()
			<< ", " << textBlock.getAuthorId() << "}";

		_document.formatBlock(textBlock, fmt);
		emit blockFormatChanged(textBlock, fmt);
	}
}

void DocumentEditor::changeBlockFormat(int start, int end, QTextBlockFormat fmt)
{
	QList<TextBlockID> blocks = _document.getBlocksBetween(start, end);

	foreach(TextBlockID textBlock, blocks) {
		qDebug().nospace() << "Local format change of block {" << textBlock.getBlockNumber()
			<< ", " << textBlock.getAuthorId() << "}";

		_document.formatBlock(textBlock, fmt);
		emit blockFormatChanged(textBlock, fmt);
	}
}

void DocumentEditor::applyBlockFormat(TextBlockID blockId, QTextBlockFormat fmt)
{
	// Early out if the local state is already aligned with the server's
	if (_document.getBlock(blockId).getFormat() == fmt)
		return;

	qDebug().nospace() << "Remote format change of block {" << blockId.getBlockNumber()
		<< ", " << blockId.getAuthorId() << "}";

	int position = _document.formatBlock(blockId, fmt);
	_textedit->applyBlockFormat(position, fmt);
}


//Symbol format
void DocumentEditor::changeSymbolFormat(int position, QTextCharFormat fmt)
{
	Symbol s = _document[position];

	qDebug().nospace() << "Local format change of character: " << s.getChar();

	_document.formatSymbol(s.getPosition(), fmt);
	emit symbolFormatChanged(s.getPosition(), fmt);
}

void DocumentEditor::applySymbolFormat(QVector<qint32> position, QTextCharFormat fmt)
{
	// Early out if the local state is already aligned with the server's
	if (_document[position].getFormat() == fmt)
		return;

	qDebug().nospace() << "Remote format change of character: " << _document[position].getChar();

	int pos = _document.formatSymbol(position, fmt);
	_textedit->applyCharFormat(pos, fmt);
}



//List editing
void DocumentEditor::listEditBlock(TextBlockID blockId, TextListID listId, QTextListFormat fmt)
{
	// Early out if the local state is already aligned with the server's
	if (_document.getBlock(blockId).getListId() == listId)
	{
		if (!listId) {	// removeBlockFromList is dispatched anyways, to ensure Qt document consistency between clients
			_textedit->removeBlockFromList(_document.getBlockPosition(blockId));
		}

		return;
	}

	qDebug().nospace() << "Remote assignment of block {" << blockId.getBlockNumber()
		<< ", " << blockId.getAuthorId() << "} to list {" << listId.getListNumber()
		<< ", " << listId.getAuthorId() << "}";;

	int blockPos = _document.getBlockPosition(blockId);

	if (!listId)
	{
		// Remove the block from its list
		_textedit->removeBlockFromList(blockPos);
	}
	else if (_document._lists.contains(listId))
	{
		// Add the block to an existing list
		_textedit->addBlockToList(blockPos, _document.getListPosition(listId));
	}
	else
	{
		// Create a new list including the specified block
		_textedit->createList(blockPos, fmt);
	}

	// Apply the remote change in the local document
	_document.editBlockList(blockId, listId, fmt);
}


// Called by textedit when creating a new list that will include the current block
void DocumentEditor::createList(int position, QTextListFormat fmt)
{
	// Create a new TextList
	TextListID newListId(_document._listCounter++, _user.getUserId());
	_document._lists.insert(newListId, TextList(newListId, fmt));

	TextList& list = _document.getList(newListId);
	TextBlock& block = _document.getBlock(_document.getBlockAt(position));

	qDebug().nospace() << "Local list creation for block {" << block.getId().getBlockNumber()
		<< ", " << block.getId().getAuthorId() << "}";

	_document.addBlockToList(block, list);

	// Notify other clients
	emit blockListChanged(block.getId(), list.getId(), fmt);
}


// Called by textedit when assigning a recently inserted block to a list
void DocumentEditor::assignBlockToList(int blockPosition, int listPosition)
{
	// Get the specified block and the list
	TextBlockID blockId = _document.getBlockAt(blockPosition);
	TextListID listId = _document.getListAt(listPosition);

	qDebug().nospace() << "Local addition of block {" << blockId.getBlockNumber()
		<< ", " << blockId.getAuthorId() << "} to list {" << listId.getListNumber()
		<< ", " << listId.getAuthorId() << "}";

	if (listId)
	{
		TextBlock& block = _document.getBlock(blockId);
		TextList& list = _document.getList(listId);

		_document.addBlockToList(block, list);

		// Notify other clients
		emit blockListChanged(blockId, listId, list.getFormat());
	}
}


// Called by textedit when a block is set to not be part of a list
void DocumentEditor::removeBlockFromList(int blockPosition)
{
	// Get the specified block
	TextBlockID blockId = _document.getBlockAt(blockPosition);

	TextBlock& block = _document.getBlock(blockId);
	if (block.getListId())
	{
		TextList& list = _document.getList(block.getListId());
		_document.removeBlockFromList(block, list);	
	}

	// Notify other clients
	emit blockListChanged(blockId, TextListID(nullptr), QTextListFormat());
}


// Handle the press of the list button in the editor
void DocumentEditor::toggleList(int start, int end, QTextListFormat fmt)
{
	// Get all the blocks inside the user selection
	QList<TextBlockID> selectedBlocks = _document.getBlocksBetween(start, end);

	// Get all the lists involved in the operation (at least one block in the selection)
	QList<TextListID> involvedLists;
	foreach(TextBlockID blockId, selectedBlocks)
	{
		TextBlock& block = _document.getBlock(blockId);
		TextListID blockListId = block.getListId();
		if (blockListId && !involvedLists.contains(blockListId))
			involvedLists.append(blockListId);
	}


	foreach(TextListID listId, involvedLists)
	{
		TextList& oldList = _document.getList(listId);
		TextListID newListId;
		QList<TextBlockID> listBlocks = _document.getOrderedListBlocks(listId);
		bool selectionBegun = false;
		bool selectionEnded = false;

		foreach(TextBlockID blockId, listBlocks)
		{
			TextBlock& block = _document.getBlock(blockId);

			if (!selectionBegun)	// the block is before the selection
			{
				// Iterate on the list blocks until we encounter the first which is part of the selection
				if (selectedBlocks.contains(blockId))
				{
					selectionBegun = true;

					// If the selected blocks are being removed from their lists
					if (fmt.style() == QTextListFormat::ListStyleUndefined)
					{
						_document.removeBlockFromList(block, oldList);

						// Notify both the editor and other clients of the removal
						_textedit->removeBlockFromList(_document.getBlockPosition(blockId));
						emit blockListChanged(blockId, TextListID(nullptr), fmt);
					}
				}
			}
			else if (selectionBegun && !selectionEnded)		// the block among those selected
			{
				if (!selectedBlocks.contains(blockId))
				{
					selectionEnded = true;

					// Create a new list which will contain all the blocks of the old list which come
					// after the end of the user selection; the new list retains its previous format
					newListId = TextListID(_document._listCounter++, _user.getUserId());
					_document._lists.insert(newListId, TextList(newListId, oldList.getFormat()));

					TextList& newList = _document.getList(newListId);
					_document.addBlockToList(block, newList);

					// Apply changes to the editor and notify others
					_textedit->createList(_document.getBlockPosition(blockId), newList.getFormat());
					emit blockListChanged(blockId, newListId, newList.getFormat());
				}
				else
				{
					if (fmt.style() == QTextListFormat::ListStyleUndefined)
					{
						// Remove the block from its list in the local editor and notify server/clients
						_document.removeBlockFromList(block, oldList);
						_textedit->removeBlockFromList(_document.getBlockPosition(blockId));
						emit blockListChanged(blockId, TextListID(nullptr), fmt);
					}
				}
			}
			else	// the block belongs to the list but is after the end of the selection
			{
				// Following blocks are added to the list that was born off the split
				TextList& newList = _document.getList(newListId);
				_document.addBlockToList(block, newList);

				// Apply changes to the editor and notify others
				_textedit->addBlockToList(_document.getBlockPosition(blockId), _document.getListPosition(newListId));
				emit blockListChanged(blockId, newListId, newList.getFormat());
			}
		}
	}


	// If a new list format was applied to the blocks
	if (fmt.style() != QTextListFormat::ListStyleUndefined)
	{
		// Create a new TextList that will contain all the selected blocks
		TextListID newListId(_document._listCounter++, _user.getUserId());
		_document._lists.insert(newListId, TextList(newListId, fmt));
		TextList& list = _document.getList(newListId);

		foreach(TextBlockID blockId, selectedBlocks)
		{
			TextBlock& block = _document.getBlock(blockId);
			if (list.isEmpty())
			{
				// The first block will take care of creating the list in the Qt editor as well
				_textedit->createList(_document.getBlockPosition(blockId), fmt);
			}
			else
			{
				// Following blocks will be appended to the list
				_textedit->addBlockToList(_document.getBlockPosition(blockId), _document.getListPosition(newListId));
			}

			_document.addBlockToList(block, list);

			// Send for each block the message for adding it to the new list
			emit blockListChanged(blockId, newListId, fmt);
		}
	}

}
