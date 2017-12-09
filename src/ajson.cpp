#include "ajson.hpp"

namespace ajson {
	const static char BackspaceCharacter = 0x08;

	static int GetLineNum(const char* s, uint32_t stop) {
		uint32_t n = 0;
		for(uint32_t i = 0;i < stop;i++) {
			if(s[i] == '\n')
				n++;
		}

		return n;
	}

	static std::string GetLine(const char* s, int line, uint32_t* pBeggining = nullptr) {
		uint32_t n = 0;
		uint32_t lineStart = 0, lineEnd = 0;
		bool started = false;

		for(uint32_t i = 0;i < strlen(s);i++) {
			if(n == line && !started) {
				lineStart = i;
				started = true;
			}

			if(n == (line + 1) && started) {
				lineEnd = i - 1;
				started = false;
				break;
			}

			if(s[i] == '\n') {
				n++;
			}
		}

		if(lineEnd == 0)
			lineEnd = strlen(s);

		if(lineStart == 0 && lineEnd == 0) {
			return std::string("");
		}

		if(pBeggining)
			(*pBeggining) = lineStart;

		return std::string(&s[lineStart], lineEnd - lineStart);
	}

	static int GetCharLength(char c) {
	    // 1 byte character 0xxx xxxx
	    // 2 byte character 110x xxxx
	    // 3 byte character 1110 xxxx
	    // 4 byte character 1111 0xxx

	    if((c & 0x80) == 0) {
	        return 1;
	    } else if((c & 0xe0) == 0xc0) {
	        return 2;
	    } else if((c & 0xf0) == 0xe0) {
	        return 3;
	    } else if((c & 0xf8) == 0xf0) {
	        return 4;
	    }
	}

	static std::string MakePointerLine(const std::string& s, uint32_t pointerPosition) {
		std::ostringstream ss;

		if(pointerPosition >= s.size())
			return std::string("");

		for(uint32_t i = 0;i < pointerPosition;) {
			if(s[i] != '\t') {
				int charLength = GetCharLength(s[i]);
				if(charLength == 1) {
					ss << ' ';
				} else {
					// Possibly buggy, maybe not all multibyte characters appear as 2 byte on monospace
					ss << "  ";
				}
				i += GetCharLength(s[i]);
			} else {
				ss << '\t';
				i++;
			}
		}

		ss << '^';
		return ss.str();
	}

	static std::string EscapeString(const std::string& s) {
		std::ostringstream ss;

		for(int i = 0;i << s.size();) {
			if(s[i] != '\\') {
				ss << s[i];
				i++;
			} else {
				if(s[i + 1] == '\n') {
					ss << '\n';
					i += 2;
				} else if(s[i + 1] == '\t') {
					ss << '\t';
					i += 2;
				} else if(s[i + 1] == '\r') {
					i += 2;
				} else if(s[i + 1] == '"') {
					ss << '"';
					i += 2;
				} else if(s[i + 1] == '/') {
					ss << '/';
					i += 2;
				} else if(s[i + 1] == '\\') {
					ss << '\\';
					i += 2;
				} else if(s[i + 1] == 'b') {
					ss << BackspaceCharacter;
					i += 2;
				}
			}
		}

		return ss.str();
	}

	static std::string DescapeString(const std::string& s) {
		std::ostringstream ss;

		for(int i = 0;i < s.size();) {
			if(s[i] == '\n') {
				ss << "\\n";
				i++;
			} else if(s[i] == '\t') {
				ss << "\\t";
				i++;
			} else if(s[i] == '"') {
				ss << "\\\"";
				i++;
			} else if(s[i] == '\\') {
				ss << "\\\\";
				i++;
			} else if(s[i] == BackspaceCharacter) {
				ss << "\\b";
				i++;
			} else {
				ss << s[i];
				i++;
			}
		}

		return ss.str();
	}

	static void ParseBlobString(char* parseBuffer, uint32_t length, std::vector<uint8_t>& blob) {
		blob.resize(0);

		// Blob strings should be checked before for correctness, so no need to do it here

		// Start from offset 2, format is b"...data..."
		uint32_t processed = 2;

		while(processed < length - 1) {
			char c = parseBuffer[processed];

			if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') {
				blob.push_back(c);
				processed++;
			} else {
				// This is our escape character '/'
				char hex0 = parseBuffer[processed + 1];
				char hex1 = parseBuffer[processed + 2];

				uint8_t tmp = 0, value = 0;
				if(hex0 >= '0' && hex0 <= '9') {
					tmp = hex0 - '0';
				} else if(hex0 >= 'a' && hex0 <= 'z') {
					tmp = hex0 - 'a';
				} else {
					tmp = hex0 - 'A';
				}

				tmp = tmp << 4;
				value |= tmp;

				if(hex1 >= '0' && hex1 <= '9') {
					tmp = hex1 - '0';
				} else if(hex1 >= 'a' && hex1 <= '9'){
					tmp = hex1 - 'a';
				} else {
					tmp = hex1 - 'A';
				}

				value |= tmp;

				blob.push_back(value);
				processed += 3;
			}
		}
	}

	static std::string GenerateBlobString(const std::vector<uint8_t>& blob) {
		std::stringstream ss;
		ss << "b\"";

		for(int i = 0;i < blob.size();i++) {
			uint8_t val = blob[i];

			if(val >= '0' && val <= '9') {
				char c = val;
				ss << c;
			} else if(val >= 'a' && val <= 'z') {
				char c = val;
				ss << c;
			} else if(val >= 'A' && val <= 'Z') {
				char c = val;
				ss << c;
			} else if(val == ' ') {
				ss << ' ';
			} else {
				// Blob escape character
				ss << '/';

				uint8_t tmp = val & 0xf0;
				tmp = tmp >> 4;
				if(tmp >= 0 && tmp <= 9) {
					char c = tmp + '0';
					ss << c;
				} else {
					char c = (tmp - 10) + 'a';
					ss << c;
				}

				tmp = val & 0x0f;
				if(tmp >= 0 && tmp <= 9) {
					char c = tmp + '0';
					ss << c;
				} else {
					char c = (tmp - 10) + 'a';
					ss << c;
				}
			}
		}

		ss << '\"';

		return ss.str();
	}

	static std::string ComposeErrorMsg(const char* buffer, const std::string& msg, uint32_t tokenLocation) {
		std::ostringstream stream;

		int lineNum = GetLineNum(buffer, tokenLocation);

		uint32_t lineBeggining = 0;
		std::string errorLine = GetLine(buffer, lineNum, &lineBeggining);
		std::string pointerLine = MakePointerLine(errorLine, tokenLocation - lineBeggining);

		stream << "At line [";
		stream << (lineNum + 1) << ':' << (tokenLocation - lineBeggining + 1) << "]: " << msg << '\n';
		stream << errorLine << '\n' << pointerLine << '\n';

		return stream.str();
	}

	static std::string ComposeUnexpectedTokenMsg(const char* buffer, uint32_t tokenLocation) {
		std::ostringstream stream;

		int lineNum = GetLineNum(buffer, tokenLocation);

		uint32_t lineBeggining = 0;
		std::string errorLine = GetLine(buffer, lineNum, &lineBeggining);
		std::string pointerLine = MakePointerLine(errorLine, tokenLocation - lineBeggining);

		stream << "At line [";
		stream << (lineNum + 1) << ':' << (tokenLocation - lineBeggining + 1);
		stream << "]: Unexpected token:\n";
		stream << errorLine << '\n' << pointerLine << '\n';

		return stream.str();
	}

	static std::string ComposeBinaryError(const char* buffer, uint32_t offset) {
		std::ostringstream ss;
		ss << "At byte " << offset << ": " << buffer << '\n';
		return ss.str();
	}

	bool IsHex16(const char* data)
	{
		if((data[0] < '0' || data[0] > '9') && (data[0] < 'a' || data[0] > 'f') && (data[0] < 'A' || data[0] > 'Z'))
			return false;
		if((data[1] < '0' || data[1] > '9') && (data[1] < 'a' || data[1] > 'f') && (data[1] < 'A' || data[1] > 'Z'))
			return false;

		return true;
	}

	Node::Node(enum NodeType type, const std::string& name) {
		m_parent = nullptr;
		m_name = name;

		m_type = type;
		if(m_type == STRING_T) {
			m_value.stringValue = new std::string();
		} else if(m_type == BLOB_T) {
			m_value.blobValue = new std::vector<uint8_t>();
		}
	}

	Node::Node(int32_t intValue, const std::string& name) {
		m_parent = nullptr;
		m_name = name;

		m_type = INT_T;
		m_value.intValue = intValue;
	}

	Node::Node(float floatValue, const std::string& name) {
		m_parent = nullptr;
		m_name = name;

		m_type = FLOAT_T;
		m_value.floatValue = floatValue;
	}

	Node::Node(bool boolValue, const std::string& name) {
		m_parent = nullptr;
		m_name = name;

		m_type = BOOL_T;
		m_value.boolValue = boolValue;
	}

	Node::Node(const std::string& stringValue, const std::string& name) {
		m_parent = nullptr;
		m_name = name;

		m_type = STRING_T;
		m_value.stringValue = new std::string(stringValue);
	}

	Node::Node(const std::vector<uint8_t>& blobValue, const std::string& name) {
		m_parent = nullptr;
		m_name = name;

		m_type = BLOB_T;
		m_value.blobValue = new std::vector<uint8_t>(blobValue);
	}

	Node::Node(std::vector<uint8_t>&& blobValue, const std::string& name) {
		m_parent = nullptr;
		m_name = name;

		m_type = BLOB_T;
		m_value.blobValue = new std::vector<uint8_t>();
		(*m_value.blobValue) = std::move(blobValue);
	}

	Node::Node(Node&& node) {
		m_parent = node.m_parent;
		node.m_parent = nullptr;

		m_name = node.m_name;

		m_type = node.m_type;
		node.m_type = NULL_T;

		if(m_type == STRING_T) {
			m_value.stringValue = node.m_value.stringValue;
			node.m_value.stringValue = nullptr;
		} else if(m_type == INT_T) {
			m_value.intValue = node.m_value.intValue;
		} else if(m_type == FLOAT_T) {
			m_value.floatValue = node.m_value.floatValue;
		} else if(m_type == BOOL_T) {
			m_value.boolValue = node.m_value.boolValue;
		} else if(m_type == BLOB_T) {
			m_value.blobValue = node.m_value.blobValue;
			node.m_value.blobValue = nullptr;
		}

		m_children = std::move(node.m_children);

		// Reset parent pointers
		for(int i = 0;i < m_children.size();i++) {
			m_children[i].m_parent = this;
		}
	}

	void Node::operator=(Node&& node) {
		m_parent = node.m_parent;
		node.m_parent = nullptr;

		m_name = std::move(node.m_name);

		m_type = node.m_type;
		node.m_type = NULL_T;

		if(m_type == STRING_T) {
			m_value.stringValue = node.m_value.stringValue;
			node.m_value.stringValue = nullptr;
		} else if(m_type == INT_T) {
			m_value.intValue = node.m_value.intValue;
		} else if(m_type == FLOAT_T) {
			m_value.floatValue = node.m_value.floatValue;
		} else if(m_type == BOOL_T) {
			m_value.boolValue = node.m_value.boolValue;
		} else if(m_type == BLOB_T) {
			m_value.blobValue = node.m_value.blobValue;
			node.m_value.blobValue = nullptr;
		}

		m_children = std::move(node.m_children);

		// Reset parent pointers
		for(int i = 0;i < m_children.size();i++) {
			m_children[i].m_parent = this;
		}
	}

	Node::~Node() {
		if(m_parent != nullptr) {
			m_parent->RemoveChild(this);
		}

		for(std::vector<Node>::iterator it = m_children.begin();it != m_children.end();it++) {
			it->m_parent = nullptr;
		}

		if(m_type == STRING_T) {
			if(m_value.stringValue != nullptr) {
				delete m_value.stringValue;
				m_value.stringValue = nullptr;
			}
		} else if(m_type == BLOB_T) {
			if(m_value.blobValue != nullptr) {
				delete m_value.blobValue;
				m_value.blobValue = nullptr;
			}
		}
	}

	uint16_t Node::Children() const {
		return m_children.size();
	}

	uint16_t Node::GetDepth() const {
		if(m_parent == nullptr)
		{
			return 0;
		}

		uint16_t depth = 1;

		Node* pParent = m_parent;
		while(pParent->m_parent != nullptr)
		{
			depth++;
			pParent = pParent->m_parent;
		}

		return depth;
	}

	Node* Node::GetParent() const {
		return m_parent;
	}

	Node* Node::AddChild(Node* node) {
		if(node->m_parent != nullptr) {
			// node already has a parent
			return nullptr;
		}

		if(node == this) {
			// can't add self as child
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add self as child\n";
			throw e;
		}

		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			//throw Exception(AST_ERROR);
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		m_children.push_back(std::move(*node));
		m_children[currentChild].m_parent = this;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::AddChild(Node* node, uint16_t index) {
		if(node->m_parent != nullptr) {
			// node already has a parent
			return nullptr;
		}

		if(node == this) {
			// can't add self as child
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add self as child\n";
			throw e;
		}

		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		std::vector<Node>::iterator it = m_children.begin();
		it += index;
		m_children.insert(it, std::move(*node));
		m_children[index].m_parent = this;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[index];
	}

	void Node::RemoveChild(uint16_t n) {
		m_children[n].m_parent = nullptr;
		m_children.erase(m_children.begin() + n);

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}
	}

	void Node::RemoveChild(Node* node) {
		for(std::vector<Node>::iterator it = m_children.begin();it != m_children.end();it++) {
			// node is a child of this
			if(node == &(*it)) {
				node->m_parent = nullptr;
				m_children.erase(it);
				break;
			}
		}

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}
	}

	void Node::RemoveAllChildren() {
		for(std::vector<Node>::iterator it = m_children.begin();it != m_children.end();it++) {
			it->m_parent = nullptr;
		}

		m_children.resize(0);
	}

	Node* Node::CreateChild(NodeType t, const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node nullNode;
		m_children.push_back(std::move(nullNode));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].SetType(t);
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateObject(const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node node(OBJECT_T);
		m_children.push_back(std::move(node));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateArray(const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node node(ARRAY_T);
		m_children.push_back(std::move(node));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateInt(int32_t intValue, const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node nullNode;
		m_children.push_back(std::move(nullNode));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].SetInt(intValue);
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateFloat(float floatValue, const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node nullNode;
		m_children.push_back(std::move(nullNode));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].SetFloat(floatValue);
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateBool(bool boolValue, const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node nullNode;
		m_children.push_back(std::move(nullNode));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].SetBool(boolValue);
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateString(const std::string& stringValue, const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node nullNode;
		m_children.push_back(std::move(nullNode));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].SetString(stringValue);
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateBlob(const std::vector<uint8_t>& blobValue, const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node nullNode;
		m_children.push_back(std::move(nullNode));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].SetBlob(blobValue);
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateBlob(std::vector<uint8_t>&& blobValue, const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node nullNode;
		m_children.push_back(std::move(nullNode));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].SetBlob(std::move(blobValue));
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateNull(const std::string& name) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node nullNode;
		m_children.push_back(std::move(nullNode));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].m_name = name;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	Node* Node::CreateComment(const std::string& value) {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			Exception e(AST_ERROR);
			e.m_node = this;
			e.m_errorString = "Cannot add child to non-container type\n";
			throw e;
		}

		uint32_t currentChild = m_children.size();
		Node nullNode;
		m_children.push_back(std::move(nullNode));
		m_children[currentChild].m_parent = this;
		m_children[currentChild].m_name = value;
		m_children[currentChild].m_type = COMMENT_T;

		// Reset parent pointer
		for(int i = 0;i < m_children.size();i++) {
			Node* childNode = &m_children[i];
			for(int j = 0;j < childNode->m_children.size();j++) {
				childNode->m_children[j].m_parent = childNode;
			}
		}

		return &m_children[currentChild];
	}

	void Node::SetType(NodeType t) {
		// If same type make no changes
		if(m_type == t) {
			return;
		}

		if(m_type == OBJECT_T || m_type == ARRAY_T) {
			RemoveAllChildren();
		}

		if(m_type == STRING_T) {
			if(m_value.stringValue != nullptr) {
				delete m_value.stringValue;
			}
		} else if(m_type == BLOB_T) {
			if(m_value.blobValue != nullptr) {
				delete m_value.blobValue;
			}
		}

		if(t == STRING_T) {
			m_value.stringValue = new std::string();
		} else if(t == BLOB_T) {
			m_value.blobValue = new std::vector<uint8_t>();
		}

		m_type = t;
	}

	Node* Node::GetChild(uint16_t child) const {
		if(m_type != OBJECT_T && m_type != ARRAY_T) {
			return nullptr;
		}

		if(child > m_children.size()) {
			return nullptr;
		}

		return const_cast<Node *>(&m_children[child]);
	}

	Node* Node::GetChild(const std::string& name) const {
		if(m_type != OBJECT_T)
		{
			return nullptr;
		}

		Node* pNode = nullptr;

		for(std::vector<Node>::const_iterator it = m_children.begin();it != m_children.end();it++) {
			if(it->GetName() == name) {
				pNode = const_cast<Node *>(&(*it));
			}
		}
		return pNode;
	}

	Node* Node::GetFirstChild() const {
		if(m_children.size() == 0) {
			return nullptr;
		} else {
			return const_cast<Node *>(&m_children[0]);
		}
	}

	Node* Node::GetLastChild() const {
		size_t nChildren = m_children.size();

		if(nChildren == 0) {
			return nullptr;
		} else {
			return const_cast<Node *>(&m_children[nChildren - 1]);
		}
	}

	void Node::SetName(const std::string& name) {
		m_name = name;
	}

	bool Node::GetBool() const {
		return m_value.boolValue;
	}

	void Node::SetBool(bool val) {
		SetType(BOOL_T);
		m_value.boolValue = val;
	}

	std::string Node::GetString() const {
		if(m_type == STRING_T) {
			return *m_value.stringValue;
		} else {
			return std::string("");
		}
	}

	void Node::SetString(const std::string& val) {
		if(m_type != STRING_T) {
			SetType(STRING_T);
			(*m_value.stringValue) = val;
		} else {
			(*m_value.stringValue) = val;
		}
	}

	std::vector<uint8_t>* Node::GetBlob() const {
		if(m_type == BLOB_T) {
			return m_value.blobValue;
		} else {
			return nullptr;
		}
	}

	void Node::SetBlob(const std::vector<uint8_t>& blobValue) {
		if(m_type != BLOB_T) {
			SetType(BLOB_T);
			(*m_value.blobValue) = blobValue;
		} else {
			(*m_value.blobValue) = blobValue;
		}
	}

	void Node::SetBlob(std::vector<uint8_t>&& blobValue) {
		if(m_type != BLOB_T) {
			SetType(BLOB_T);
			(*m_value.blobValue) = std::move(blobValue);
		} else {
			(*m_value.blobValue) = std::move(blobValue);
		}
	}

	std::string Node::GetComment() const {
		if(m_type == COMMENT_T) {
			return m_name;
		} else {
			return std::string("");
		}
	}

	void Node::SetComment(const std::string& val) {
		if(m_type != COMMENT_T) {
			SetType(COMMENT_T);
			m_name = val;
		} else {
			m_name = val;
		}
	}

	int32_t Node::GetInt() const {
		if(m_type == INT_T) {
			return m_value.intValue;
		} else if(m_type == FLOAT_T) {
			return static_cast<int32_t>(m_value.floatValue);
		} else {
			return 0;
		}
	}

	void Node::SetInt(int32_t val) {
		SetType(INT_T);
		m_value.intValue = val;
	}

	float Node::GetFloat() const {
		if(m_type == FLOAT_T) {
			return m_value.floatValue;
		} else if(m_type == INT_T) {
			return static_cast<float>(m_value.intValue);
		} else {
			return 0.0f;
		}
	}

	void Node::SetFloat(float val) {
		SetType(FLOAT_T);
		m_value.floatValue = val;
	}

	Node::iterator Node::begin() const {
		iterator it;
		it.m_node = const_cast<Node *>(this);
		it.m_child = 0;
		return it;
	}

	Node::iterator Node::end() const {
		Node::iterator it;
		it.m_node = const_cast<Node *>(this);
		it.m_child = m_children.size();
		return it;
	}

	enum JsonTokens
	{
		JSON_OBJ_BEGIN,
		JSON_OBJ_END,
		JSON_ARR_BEGIN,
		JSON_ARR_END,
		JSON_NAME_SEP,
		JSON_VALUE_SEP,
		JSON_STRING,
		JSON_INT,
		JSON_FLOAT,
		JSON_TRUE,
		JSON_FALSE,
		JSON_NULL,
		JSON_BLOB,
		JSON_COMMENT
	};

	class JsonToken
	{
	public:
		JsonToken(){};
		JsonToken(enum JsonTokens t, uint32_t loc, uint32_t sz)
		{
			type = t;
			location = loc;
			size = sz;
		}

		JsonToken(enum JsonTokens t, uint32_t loc)
		{
			type = t;
			location = loc;
			size = 1;
		}

		void set(enum JsonTokens t, uint32_t loc, uint32_t sz)
		{
			type = t;
			location = loc;
			size = sz;
		}

		void set(enum JsonTokens t, uint32_t loc)
		{
			type = t;
			location = loc;
			size = 1;
		}

		enum JsonTokens type;
		uint32_t location, size;
	};

	void ParseJsonNode(std::vector<JsonToken>& tokens, uint32_t& current, Node* node, char* parseBuffer, CommentPolicy commentPolicy) {
		uint32_t temp;
		bool comma = false;
		bool started = false;

		uint32_t containerStart = current;

		if(node->GetType() == OBJECT_T)
		{
			while(tokens[current].type != JSON_OBJ_END)
			{
				// Check for end of data
				if(current >= tokens.size()) {
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected end of data parsing object:", tokens[tokens.size() - 1].location));
				}

				if(tokens[current].type == JSON_COMMENT) {
					if(commentPolicy == ACCEPT_COMMENTS) {
						Node tempNode(COMMENT_T);

						parseBuffer[tokens[current].location + tokens[current].size - 2] = '\0';
						tempNode.SetComment(std::string(&parseBuffer[tokens[current].location + 2]));
						parseBuffer[tokens[current].location + tokens[current].size - 2] = '*';

						node->AddChild(&tempNode);
					}

					current++;
				} else if(tokens[current].type == JSON_STRING) {
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token after value, was expecting ',':", tokens[current].location));
					}
					comma = false;
					started = true;
					if(current + 1 >= tokens.size()) {
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ':' after node name", tokens[current].location));
					}
					if(current + 2 >= tokens.size())
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected value after ':'", tokens[current + 1].location));
					}
					if(tokens[current + 1].type != JSON_NAME_SEP)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token after node name, was expecting ':':", tokens[current + 1].location));
					}
					if(current + 3 >= tokens.size())
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unterminated Object, expected '}'", tokens[containerStart].location));
					}
					if(tokens[current + 2].type == JSON_OBJ_BEGIN)
					{
						Node tempNode(OBJECT_T);
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
						tempNode.SetName(std::string(&parseBuffer[tokens[current].location + 1]));
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';
						current += 3;

						Node* newNode = node->AddChild(&tempNode);
						ParseJsonNode(tokens, current, newNode, parseBuffer, commentPolicy);
					} else if(tokens[current + 2].type == JSON_ARR_BEGIN)
					{
						Node tempNode(ARRAY_T);
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
						tempNode.SetName(std::string(&parseBuffer[tokens[current].location + 1]));
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';
						current += 3;

						Node* newNode = node->AddChild(&tempNode);
						ParseJsonNode(tokens, current, newNode, parseBuffer, commentPolicy);
					} else if(tokens[current + 2].type == JSON_STRING)
					{
						Node tempNode(STRING_T);

						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
						tempNode.SetName(std::string(&parseBuffer[tokens[current].location + 1]));
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';

						parseBuffer[tokens[current + 2].location + tokens[current + 2].size + 1] = '\0';
						tempNode.SetString(std::string(&parseBuffer[tokens[current + 2].location + 1]));
						parseBuffer[tokens[current + 2].location + tokens[current + 2].size + 1] = '\"';

						current += 3;
						node->AddChild(&tempNode);
					} else if(tokens[current + 2].type == JSON_INT)
					{
						Node tempNode(INT_T);

						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
						tempNode.SetName(std::string(&parseBuffer[tokens[current].location + 1]));
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';

						char tempChar = parseBuffer[tokens[current + 2].location + tokens[current + 2].size + 1];
						parseBuffer[tokens[current + 2].location + tokens[current + 2].size + 1] = '\0';
						int32_t tempInt;
						sscanf(&parseBuffer[tokens[current + 2].location], "%i", &tempInt);
						tempNode.SetInt(tempInt);
						parseBuffer[tokens[current + 2].location + tokens[current + 2].size + 1] = tempChar;

						current += 3;
						node->AddChild(&tempNode);
					} else if(tokens[current + 2].type == JSON_FLOAT)
					{
						Node tempNode(FLOAT_T);

						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
						tempNode.SetName(std::string(&parseBuffer[tokens[current].location + 1]));
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';

						float tempFloat;
						sscanf(&parseBuffer[tokens[current + 2].location], "%f", &tempFloat);
						tempNode.SetFloat(tempFloat);

						current += 3;
						node->AddChild(&tempNode);
					} else if(tokens[current + 2].type == JSON_TRUE)
					{
						Node tempNode(BOOL_T);
						tempNode.SetBool(true);

						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
						tempNode.SetName(std::string(&parseBuffer[tokens[current].location + 1]));
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';

						current += 3;
						node->AddChild(&tempNode);
					} else if(tokens[current + 2].type == JSON_FALSE)
					{
						Node tempNode(BOOL_T);
						tempNode.SetBool(false);

						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
						tempNode.SetName(std::string(&parseBuffer[tokens[current].location + 1]));
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';

						current += 3;
						node->AddChild(&tempNode);
					} else if(tokens[current + 2].type == JSON_NULL)
					{
						Node tempNode(NULL_T);

						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
						tempNode.SetName(std::string(&parseBuffer[tokens[current].location + 1]));
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';

						current += 3;
						node->AddChild(&tempNode);
					} else if(tokens[current + 2].type == JSON_BLOB) {
						Node tempNode(BLOB_T);

						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
						tempNode.SetName(std::string(&parseBuffer[tokens[current].location + 1]));
						parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';

						std::vector<uint8_t> blob;
						ParseBlobString(&parseBuffer[tokens[current + 2].location], tokens[current + 2].size, blob);
						tempNode.SetBlob(std::move(blob));

						current += 3;
						node->AddChild(&tempNode);
					} else
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token:", tokens[current + 2].location));
					}
				} else if(tokens[current].type == JSON_VALUE_SEP)
				{
					if(node->Children() == 0)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected ',' before any other nodes:", tokens[current].location));
					}

					if(current + 1 >= tokens.size())
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected end of data after ',':", tokens[current].location));
					}
					if(tokens[current + 1].type == JSON_VALUE_SEP)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token after ',', was expecting node name:", tokens[current + 1].location));
					}
					if(tokens[current + 1].type != JSON_STRING) {
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token after ',', was expecting node name:", tokens[current + 1].location));
					}

					current++;
					comma = true;
				} else
				{
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token:", tokens[current].location));
				}
			}
		} else
		{
			bool comma = false;
			bool started = false;
			while(tokens[current].type != JSON_ARR_END)
			{
				if(current == tokens.size())
				{
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected end of data:", tokens[current - 1].location));
				}

				if(tokens[current].type == JSON_COMMENT) {
					if(commentPolicy == ACCEPT_COMMENTS) {
						Node tempNode(COMMENT_T);

						parseBuffer[tokens[current].location + tokens[current].size - 2] = '\0';
						tempNode.SetComment(std::string(&parseBuffer[tokens[current].location + 2]));
						parseBuffer[tokens[current].location + tokens[current].size - 2] = '*';

						node->AddChild(&tempNode);
					}

					current++;
				} else if(tokens[current].type == JSON_STRING)
				{
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ','", tokens[current].location));
					}

					comma = false;
					started = true;

					Node tempNode(STRING_T);
					parseBuffer[tokens[current].location + tokens[current].size + 1] = '\0';
					tempNode.SetString(std::string(&parseBuffer[tokens[current].location + 1]));
					parseBuffer[tokens[current].location + tokens[current].size + 1] = '\"';

					current++;
					node->AddChild(&tempNode);
				} else if(tokens[current].type == JSON_INT)
				{
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ','", tokens[current].location));
					}

					comma = false;
					started = true;

					Node tempNode(INT_T);
					char tempChar = parseBuffer[tokens[current].location + tokens[current].size];
					parseBuffer[tokens[current].location + tokens[current].size] = '\0';
					int32_t tempInt;
					sscanf(&parseBuffer[tokens[current].location], "%i", &tempInt);
					parseBuffer[tokens[current].location + tokens[current].size] = tempChar;
					tempNode.SetInt(tempInt);

					current++;
					node->AddChild(&tempNode);
				} else if(tokens[current].type == JSON_FLOAT)
				{
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ','", tokens[current].location));
					}

					comma = false;
					started = true;

					Node tempNode(FLOAT_T);
					char tempChar = parseBuffer[tokens[current].location + tokens[current].size];
					parseBuffer[tokens[current].location + tokens[current].size] = '\0';
					float tempFloat;
					sscanf(&parseBuffer[tokens[current].location], "%f", &tempFloat);
					parseBuffer[tokens[current].location + tokens[current].size] = tempChar;
					tempNode.SetFloat(tempFloat);

					current++;
					node->AddChild(&tempNode);
				} else if(tokens[current].type == JSON_TRUE)
				{
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ','", tokens[current].location));
					}

					comma = false;
					started = true;

					Node tempNode(BOOL_T);
					tempNode.SetBool(true);

					current++;
					node->AddChild(&tempNode);
				} else if(tokens[current].type == JSON_FALSE)
				{
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ','", tokens[current].location));
					}

					comma = false;
					started = true;

					Node tempNode(BOOL_T);
					tempNode.SetBool(false);

					current++;
					node->AddChild(&tempNode);
				} else if(tokens[current].type == JSON_NULL)
				{
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ','", tokens[current].location));
					}

					comma = false;
					started = true;

					Node tempNode(NULL_T);

					current++;
					node->AddChild(&tempNode);
				} else if(tokens[current].type == JSON_BLOB) {
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ','", tokens[current].location));
					}

					comma = false;
					started = true;

					Node tempNode(BLOB_T);

					std::vector<uint8_t> blob;
					ParseBlobString(&parseBuffer[tokens[current + 2].location], tokens[current + 2].size, blob);
					tempNode.SetBlob(std::move(blob));

					current++;
					node->AddChild(&tempNode);
				} else if(tokens[current].type == JSON_OBJ_BEGIN)
				{
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ','", tokens[current].location));
					}

					comma = false;
					started = true;

					Node tempNode(OBJECT_T);
					current++;

					Node* newNode = node->AddChild(&tempNode);
					ParseJsonNode(tokens, current, newNode, parseBuffer, commentPolicy);
				} else if(tokens[current].type == JSON_ARR_BEGIN)
				{
					if(node->Children() > 0 && !comma && started)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Expected ','", tokens[current].location));
					}

					comma = false;
					started = true;

					Node tempNode(ARRAY_T);
					current++;

					Node* newNode = node->AddChild(&tempNode);
					ParseJsonNode(tokens, current, newNode, parseBuffer, commentPolicy);
				} else if(tokens[current].type == JSON_VALUE_SEP)
				{
					if(comma == true)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected ','", tokens[current].location));
					}

					if(node->Children() == 0)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected ',' before any other nodes:", tokens[current].location));
					}

					if(current + 1 >= tokens.size())
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected end of data after ',':", tokens[current].location));
					}
					if(tokens[current + 1].type == JSON_VALUE_SEP)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token after ',':", tokens[current + 1].location));
					}

					comma = true;
					current++;
				} else
				{
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token:", tokens[current].location));
				}
			}
		}

		current++;
	}

	void ParseNumber(char* parseBuffer, std::vector<JsonToken>& tokens, uint32_t& processed, uint32_t length) {
		bool isFloat = false;
		uint32_t start = processed;
		while(processed != length)
		{
			if(parseBuffer[processed] == '.')
			{
				isFloat = true;
			}
			if((parseBuffer[processed] < '0' || parseBuffer[processed] > '9') && parseBuffer[processed] != '.' && parseBuffer[processed] != 'e' && parseBuffer[processed] != 'E' && parseBuffer[processed] != '-' && parseBuffer[processed] != '+')
			{
				break;
			}
			processed++;
		}
		JsonToken token;
		if(isFloat)
		{
			token.set(JSON_FLOAT, start, processed - start);
			char tempChar = parseBuffer[processed];
			parseBuffer[processed] = '\0';
			float tempFloat;
			if(!sscanf(&parseBuffer[start], "%f", &tempFloat))
			{
				throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Error parsing number:", start));
			}
			parseBuffer[processed] = tempChar;
			tokens.push_back(token);
		} else
		{
			token.set(JSON_INT, start, processed - start);
			char tempChar = parseBuffer[processed];
			parseBuffer[processed] = '\0';
			int32_t tempInt;
			if(!sscanf(&parseBuffer[start], "%i", &tempInt))
			{
				throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Error parsing number:", start));
			}
			parseBuffer[processed] = tempChar;
			tokens.push_back(token);
		}
	}

	static void ParseBlob(char* parseBuffer, std::vector<JsonToken>& tokens, uint32_t& processed, uint32_t length) {
		uint32_t start = processed;

		// First character is supposed to be 'b', so increment 'processed'
		processed++;

		// Next character should be '"'
		if(parseBuffer[processed] != '\"') {
			throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Error parsing blob, was expecting '\"' after b:", processed));
		}

		processed++;
		bool closed = false;

		while(processed != length) {
			char c = parseBuffer[processed];

			// If [a-zA-Z0-9] continue
			if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') {
				processed++;
				continue;
			}

			if(c == '\"') {
				processed++;
				closed = true;
				break;
			}

			if(c == '/') {
				if(processed + 2 >= length) {
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected end of data parsing blob:", processed));
				}

				if(!IsHex16(&parseBuffer[processed + 1])) {
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unknown blob character, expected Hex16:", processed));
				}

				processed += 3;
				continue;
			}

			throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unknown blob character:", processed));
		}

		if(!closed) {
			throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unclosed blob:", start));
		}

		JsonToken token(JSON_BLOB, start, processed - start);
		tokens.push_back(token);
	}

	static void ParseComment(char* parseBuffer, std::vector<JsonToken>& tokens, uint32_t& processed, uint32_t length) {
		uint32_t start = processed;
		uint32_t end = start;

		if(parseBuffer[processed + 1] != '*') {
			throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token, was expecting '*'", start + 1));
		}

		bool closed = false;

		processed += 2;
		while(processed != length) {
			if(parseBuffer[processed] == '*') {
				if(processed + 1 >= length) {
					// Unterminated comment
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unterminated comment", start));
				}

				if(parseBuffer[processed + 1] == '/') {
					// Comment end
					closed = true;
					processed += 2;
					end = processed;
					break;
				}
			} else {
				processed++;
			}
		}

		if(!closed) {
			throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unterminated comment", start));
		}

		JsonToken token(JSON_COMMENT, start, end - start);
		tokens.push_back(token);
	}

	static void ParseTokens(std::vector<JsonToken>& tokens, char* parseBuffer, CommentPolicy commentPolicy) {
		uint32_t processed = 0;
		uint32_t length = strlen(parseBuffer);

		JsonToken token;

		uint32_t stringStart, stringEnd;

		while(processed != length)
		{
			switch(parseBuffer[processed])
			{
			case '{':
				token.set(JSON_OBJ_BEGIN, processed);
				tokens.push_back(token);
				processed++;
				break;
			case '}':
				token.set(JSON_OBJ_END, processed);
				tokens.push_back(token);
				processed++;
				break;
			case '[':
				token.set(JSON_ARR_BEGIN, processed);
				tokens.push_back(token);
				processed++;
				break;
			case ']':
				token.set(JSON_ARR_END, processed);
				tokens.push_back(token);
				processed++;
				break;
			case ',':
				token.set(JSON_VALUE_SEP, processed);
				tokens.push_back(token);
				processed++;
				break;
			case ':':
				token.set(JSON_NAME_SEP, processed);
				tokens.push_back(token);
				processed++;
				break;
			case '"':
				stringStart = processed;
				processed++;
				while(processed != length)
				{
					if(parseBuffer[processed] == '"')
					{
						token.set(JSON_STRING, stringStart, processed - stringStart - 1);
						tokens.push_back(token);
						processed++;
						break;
					}

					if(processed + 1 >= length)
					{
						throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unterminated string:", processed));
					}

					if(parseBuffer[processed] == '\\')
					{
						if(processed + 2 >= length)
						{
							throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unterminated string:", processed + 1));
						}
						if(parseBuffer[processed + 1] == 'f' || parseBuffer[processed + 1] == 'n' || parseBuffer[processed + 1] == 'r' || parseBuffer[processed + 1] == 't' || parseBuffer[processed + 1] == 'b' || parseBuffer[processed + 1] == '\\' || parseBuffer[processed + 1] == '/' || parseBuffer[processed + 1] == '"')
						{
							processed += 2;
						} else if(parseBuffer[processed + 1] == 'u')
						{
							if(processed + 6 >= length)
							{
								throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected end of data when parsing hexadecimal escape sequence:", processed + 1));
							}
							if(!IsHex16(&parseBuffer[processed + 2]))
							{
								throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Escape sequence is not hexadecimal:", processed + 1));
							}
							processed += 6;
						} else
						{
							throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unrecognized escape sequence", processed));
						}
					} else if(parseBuffer[processed] == '\n' || parseBuffer[processed] == '\f' || parseBuffer[processed] == '\r' || parseBuffer[processed] == '\t' || parseBuffer[processed] == '\b')
					{

					} else
					{
						processed++;
					}
				}
				break;
			case 't':
				if(processed + 4 >= length)
				{
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected end of data:", length));
				}
				if(parseBuffer[processed + 1] != 'r' || parseBuffer[processed + 2] != 'u' || parseBuffer[processed + 3] != 'e')
				{
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token, was expecting 'true':", processed));
				}
				token.set(JSON_TRUE, processed, 4);
				tokens.push_back(token);
				processed += 4;
				break;
			case 'f':
				if(processed + 5 >= length)
				{
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected end of data:", length));
				}
				if(parseBuffer[processed + 1] != 'a' || parseBuffer[processed + 2] != 'l' || parseBuffer[processed + 3] != 's' || parseBuffer[processed + 4] != 'e')
				{
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token, was expecting 'false':", processed));
				}
				token.set(JSON_FALSE, processed, 5);
				tokens.push_back(token);
				processed += 5;
				break;
			case 'n':
				if(processed + 4 >= length)
				{
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected end of data:", length));
				}
				if(parseBuffer[processed + 1] != 'u' || parseBuffer[processed + 2] != 'l' || parseBuffer[processed + 3] != 'l')
				{
					throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token, was expecting 'null':", processed));
				}
				token.set(JSON_NULL, processed, 4);
				tokens.push_back(token);
				processed += 4;
				break;
			case 'b':
				ParseBlob(parseBuffer, tokens, processed, length);
				break;
			case '0':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '1':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '2':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '3':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '4':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '5':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '6':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '7':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '8':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '9':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case '-':
				ParseNumber(parseBuffer, tokens, processed, length);
				break;
			case ' ':
				processed++;
				break;
			case '\n':
				processed++;
				break;
			case '\r':
				processed++;
				break;
			case '\t':
				processed++;
				break;
			case '\f':
				processed++;
				break;
			case '/':
				// Comment
				if(commentPolicy == NO_COMMENTS) {
					throw Exception(PARSER_ERROR, ComposeUnexpectedTokenMsg(parseBuffer, processed));
				}

				ParseComment(parseBuffer, tokens, processed, length);
				break;
			default:
				throw Exception(PARSER_ERROR, ComposeUnexpectedTokenMsg(parseBuffer, processed));
				break;
			}
		}
	}

	Node* ParseJson(const std::string& _parseBuffer, CommentPolicy commentPolicy) {
		std::vector<JsonToken> tokens;
		char* parseBuffer = const_cast<char*>(_parseBuffer.c_str());
		ParseTokens(tokens, parseBuffer, commentPolicy);

		if(tokens.size() == 0)
		{
			return nullptr;
		}

		uint32_t currentToken = 1;

		Node* rootNode;

		if(tokens[0].type == JSON_OBJ_BEGIN)
		{
			rootNode = new Node(OBJECT_T);
		} else if(tokens[0].type == JSON_ARR_BEGIN)
		{
			rootNode = new Node(ARRAY_T);
		} else
		{
			throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token, was expecting '{' or '[':", tokens[0].location));
		}

		ParseJsonNode(tokens, currentToken, rootNode, parseBuffer, commentPolicy);

		//if(currentToken + 1 < tokens.size()) {
		//	throw
		//}

		return rootNode;
	}

	Node* ParseJsonFile(const std::string& filename, CommentPolicy commentPolicy) {
		FILE* fp = fopen(filename.c_str(), "r");
		if(fp == nullptr)
		{
			std::string errorString = "Could not read from file '" + filename + "'.\n";
			throw Exception(IO_ERROR, errorString);
		}

		fseek(fp, 0, SEEK_END);
		size_t length = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		char* parseBuffer = new char[length + 1];
		parseBuffer[length] = '\0';
		fread(parseBuffer, 1, length, fp);

		fclose(fp);

		std::vector<JsonToken> tokens;
		ParseTokens(tokens, parseBuffer, commentPolicy);

		if(tokens.size() == 0)
		{
			return nullptr;
		}

		uint32_t currentToken = 1;

		Node* rootNode;

		if(tokens[0].type == JSON_OBJ_BEGIN)
		{
			rootNode = new Node(OBJECT_T);
		} else if(tokens[0].type == JSON_ARR_BEGIN)
		{
			rootNode = new Node(ARRAY_T);
		} else
		{
			throw Exception(PARSER_ERROR, ComposeErrorMsg(parseBuffer, "Unexpected token, was expecting '{' or '[':", tokens[0].location));
		}

		ParseJsonNode(tokens, currentToken, rootNode, parseBuffer, commentPolicy);

		delete parseBuffer;
		return rootNode;
	}

	template <enum JsonOutput formatStyle> void PrintJson(const Node& node, std::string& buf, uint16_t depth, CommentPolicy commentPolicy);

	template<> void PrintJson<JSON_SPACED>(const Node& node, std::string& buf, uint16_t depth, CommentPolicy commentPolicy) {
		const char trueString[] = "true";
		const char falseString[] = "false";
		const char nullString[] = "null";

		char charBuffer[2];
		charBuffer[1] = '\0';

		const uint32_t NumBufferSize = 64;

		char numBuffer[NumBufferSize];
		memset(numBuffer, '\0', NumBufferSize);

		Node* pParent = node.GetParent();

		if(commentPolicy != ACCEPT_COMMENTS && node.GetType() == COMMENT_T) {
			return;
		}

		if(node.GetParent() != nullptr) {
			if(node.GetParent()->GetType() == OBJECT_T)
			{
				for(uint16_t i = 0;i < depth;i++)
				{
					buf += "\t";
				}

				if(node.GetType() != COMMENT_T) {
					buf += "\"";
					buf += DescapeString(node.GetName());
					buf += "\" : ";
				}
			} else {
				for(uint16_t i = 0;i < depth;i++)
				{
					buf += "\t";
				}
			}
		}

		switch(node.GetType())
		{
		case OBJECT_T:
			buf += "{\n";
			for(uint16_t i = 0;i < node.Children();i++)
			{
				PrintJson<JSON_SPACED>(*node.GetChild(i), buf, depth + 1, commentPolicy);

				if(commentPolicy == ACCEPT_COMMENTS) {
					if(i == (node.Children() - 1) || node.GetChild(i)->GetType() == COMMENT_T) {
						buf += "\n";
					} else {
						buf += ",\n";
					}
				} else {
					if(node.GetChild(i)->GetType() != COMMENT_T) {
						if(i == node.Children() - 1) {
							buf += "\n";
						} else {
							buf += ",\n";
						}
					}
				}
			}
			for(uint16_t i = 0;i < depth;i++)
			{
				buf += "\t";
			}

			buf += "}";
			break;
		case ARRAY_T:
			buf += "[\n";
			for(uint16_t i = 0;i < node.Children();i++)
			{
				PrintJson<JSON_SPACED>(*node.GetChild(i), buf, depth + 1, commentPolicy);

				if(commentPolicy == ACCEPT_COMMENTS) {
					if(i == (node.Children() - 1) || node.GetChild(i)->GetType() == COMMENT_T) {
						buf += "\n";
					} else {
						buf += ",\n";
					}
				} else {
					if(node.GetChild(i)->GetType() != COMMENT_T) {
						if(i == (node.Children() - 1)) {
							buf += "\n";
						} else {
							buf += ",\n";
						}
					}
				}
			}
			for(uint16_t i = 0;i < depth;i++)
			{
				buf += "\t";
			}

			buf += "]";
			break;
		case STRING_T:
			buf += "\"";
			buf += DescapeString(node.GetString());
			buf += "\"";
			break;
		case COMMENT_T:
			buf += "/*";
			buf += node.GetComment();
			buf += "*/";
			break;
		case INT_T:
			sprintf(numBuffer, "%i", node.GetInt());
			buf += numBuffer;
			break;
		case FLOAT_T:
			{
				sprintf(numBuffer, "%f", node.GetFloat());

				uint32_t decimal = 0;
				uint32_t lastNonZero = 0;

				// Remove unnecessary zeros after decimal
				for(uint32_t i = 0;i < NumBufferSize;i++) {
					if(numBuffer[i] == '.') {
						decimal = i + 1;
						lastNonZero = decimal;
						break;
					}
				}

				for(uint32_t i = decimal;i < NumBufferSize;i++) {
					if(numBuffer[i] > '0') {
						lastNonZero = i;
					}
				}

				numBuffer[lastNonZero + 1] = '\0';

				buf += numBuffer;
			}
			break;
		case BOOL_T:
			if(node.GetBool() == true) {
				buf += trueString;
			} else {
				buf += falseString;
			}
			break;
		case NULL_T:
			buf += nullString;
			break;
		case BLOB_T:
			std::vector<uint8_t>* pBlob = node.GetBlob();
			buf += GenerateBlobString(*pBlob);
			break;
		}
	}

	template<> void PrintJson<JSON_COMPACT>(const Node& node, std::string& buf, uint16_t depth, CommentPolicy commentPolicy) {
		const char trueString[] = "true";
		const char falseString[] = "false";
		const char nullString[] = "null";

		char charBuffer[2];
		charBuffer[1] = '\0';

		const uint32_t NumBufferSize = 64;

		char numBuffer[NumBufferSize];
		memset(numBuffer, '\0', NumBufferSize);

		Node* pParent = node.GetParent();

		if(commentPolicy != ACCEPT_COMMENTS && node.GetType() == COMMENT_T) {
			return;
		}

		if(node.GetParent() != nullptr) {
			if(node.GetParent()->GetType() == OBJECT_T && node.GetType() != COMMENT_T)
			{
				buf += "\"";
				buf += DescapeString(node.GetName());
				buf += "\":";
			}
		}

		switch(node.GetType())
		{
		case OBJECT_T:
			buf += "{";
			for(uint16_t i = 0;i < node.Children();i++)
			{
				PrintJson<JSON_COMPACT>(*node.GetChild(i), buf, 0, commentPolicy);

				if(i != (node.Children() - 1) && node.GetChild(i)->GetType() != COMMENT_T) {
					buf += ",";
				}
			}

			buf += "}";
			break;
		case ARRAY_T:
			buf += "[";
			for(uint16_t i = 0;i < node.Children();i++)
			{
				PrintJson<JSON_COMPACT>(*node.GetChild(i), buf, 0, commentPolicy);

				if(i != (node.Children() - 1) && node.GetChild(i)->GetType() != COMMENT_T) {
					buf += ",";
				}
			}

			buf += "]";
			break;
		case STRING_T:
			buf += "\"";
			buf += DescapeString(node.GetString());
			buf += "\"";
			break;
		case COMMENT_T:
			buf += "/*";
			buf += node.GetComment();
			buf += "*/";
			break;
		case INT_T:
			sprintf(numBuffer, "%i", node.GetInt());
			buf += numBuffer;
			break;
		case FLOAT_T:
			{
				sprintf(numBuffer, "%f", node.GetFloat());

				uint32_t decimal = 0;
				uint32_t lastNonZero = 0;

				// Remove unnecessary zeros after decimal
				for(uint32_t i = 0;i < NumBufferSize;i++) {
					if(numBuffer[i] == '.') {
						decimal = i + 1;
						lastNonZero = decimal;
						break;
					}
				}

				for(uint32_t i = decimal;i < NumBufferSize;i++) {
					if(numBuffer[i] > '0') {
						lastNonZero = i;
					}
				}

				numBuffer[lastNonZero + 1] = '\0';

				buf += numBuffer;
			}
			break;
		case BOOL_T:
			if(node.GetBool() == true)
			{
				buf += trueString;
			} else
			{
				buf += falseString;
			}
			break;
		case NULL_T:
			buf += nullString;
			break;
		case BLOB_T:
			std::vector<uint8_t>* pBlob = node.GetBlob();
			buf += GenerateBlobString(*pBlob);
			break;
		}
	}

	template<> std::string GenerateJson<JSON_SPACED>(const Node& node, CommentPolicy commentPolicy) {
		std::string output;
		PrintJson<JSON_SPACED>(node, output, 0, commentPolicy);

		return output;
	}

	template<> std::string GenerateJson<JSON_COMPACT>(const Node& node, CommentPolicy commentPolicy) {
		std::string output;
		PrintJson<JSON_COMPACT>(node, output, 0, commentPolicy);

		return output;
	}

	template<> void GenerateJsonFile<JSON_SPACED>(const Node& node, const std::string& filename, CommentPolicy commentPolicy) {
		FILE* fp = fopen(filename.c_str(), "w");
		if(fp == nullptr)
		{
			std::string errorString = "Could not open file '" + filename + "' for writing.\n";
			throw Exception(IO_ERROR, errorString);
		}
		std::string buffer;
		try
		{
			buffer = GenerateJson<JSON_SPACED>(node, commentPolicy);
		} catch(Exception e)
		{
			fclose(fp);
			throw e;
		}
		fwrite(buffer.c_str(), 1, buffer.size(), fp);
		fclose(fp);
	}

	template<> void GenerateJsonFile<JSON_COMPACT>(const Node& node, const std::string& filename, CommentPolicy commentPolicy) {
		FILE* fp = fopen(filename.c_str(), "w");
		if(fp == nullptr)
		{
			std::string errorString = "Could not open file '" + filename + "' for writing.\n";
			throw Exception(IO_ERROR, errorString);
		}
		std::string buffer;
		try
		{
			buffer = GenerateJson<JSON_COMPACT>(node, commentPolicy);
		} catch(Exception e)
		{
			fclose(fp);
			throw e;
		}
		fwrite(buffer.c_str(), 1, buffer.size(), fp);
		fclose(fp);
	}

	class BinaryReader {
	public:
		BinaryReader(BinaryBuffer* buffer) {
			m_buffer = buffer;
			m_pointer = 0;
			m_endianness = getEndianness();
		}

		~BinaryReader() {
			m_buffer = nullptr;
		}

		enum class Endianness {
		    Big = 1,
		    Little = 0
		};

		Endianness getEndianness() {
	        int temp = 1;
	        if((&temp)[0] == 1) {
	            return Endianness::Little;
	        } else {
	            return Endianness::Big;
	        }
	    }

		template <class T>
	    T swap(const T& val) {
	        T newVal = static_cast<T>(0);

	        for(size_t i = 0;i < sizeof(T);i++) {
	            ((char *)(&newVal))[i] = ((char *)(&val))[sizeof(T) - i - 1];
	        }

	        return newVal;
	    }

		bool readChar(char& value) {
			if(m_pointer + 1 > m_buffer->Size()) {
				return false;
			}

			value = m_buffer->Data()[m_pointer];
			m_pointer++;
			return true;
		}

		bool readShort(int16_t& value) {
			if(m_pointer + 2 > m_buffer->Size()) {
				return false;
			}

			value = reinterpret_cast<int16_t *>(m_buffer->Data())[m_pointer];
			if(m_endianness == Endianness::Little) {
				value = swap(value);
			}
			m_pointer += 2;
			return true;
		}

		bool readInt(int32_t& value) {
			if(m_pointer + 4 > m_buffer->Size()) {
				return false;
			}

			value = *reinterpret_cast<int32_t *>(&m_buffer->Data()[m_pointer]);
			if(m_endianness == Endianness::Little) {
				value = swap(value);
			}
			m_pointer += 4;
			return true;
		}

		bool readFloat(float& value) {
			if(m_pointer + 4 > m_buffer->Size()) {
				return false;
			}

			value = *reinterpret_cast<float *>(&m_buffer->Data()[m_pointer]);
			if(m_endianness == Endianness::Little) {
				value = swap(value);
			}
			m_pointer += 4;
			return true;
		}

		bool readString(std::string& value) {
			uint32_t start = m_pointer;
			bool ended = false;

			while(m_pointer != m_buffer->Size()) {
				if(m_buffer->Data()[m_pointer] == '\0') {
					ended = true;
					break;
				}
				m_pointer++;
			}

			if(!ended) {
				// Unterminated string
				return false;
			}

			m_pointer++;
			value = std::move(std::string(&m_buffer->Data()[start]));
			return true;
		}

		void revert() {
			m_pointer--;
		}

		uint32_t pointer() const {
			return m_pointer;
		}

		bool eof() {
			if(m_pointer == m_buffer->Size()) {
				return true;
			} else {
				return false;
			}
		}

		BinaryBuffer* m_buffer;
		uint32_t m_pointer;
		Endianness m_endianness;
	};

	class BinaryWriter {
	public:
		BinaryWriter() {
			m_endianness = getEndianness();
		}

		~BinaryWriter() {

		}

		enum class Endianness {
		    Big = 1,
		    Little = 0
		};

		Endianness getEndianness() {
	        int temp = 1;
	        if((&temp)[0] == 1) {
	            return Endianness::Little;
	        } else {
	            return Endianness::Big;
	        }
	    }

		template <class T>
	    T swap(const T& val) {
	        T newVal = static_cast<T>(0);

	        for(size_t i = 0;i < sizeof(T);i++) {
	            ((char *)(&newVal))[i] = ((char *)(&val))[sizeof(T) - i - 1];
	        }

	        return newVal;
	    }

		void writeChar(char value) {
			m_data.push_back(value);
		}

		void writeShort(int16_t value) {
			if(m_endianness == Endianness::Little) {
				value = swap(value);
			}

			m_data.push_back(reinterpret_cast<char *>(&value)[0]);
			m_data.push_back(reinterpret_cast<char *>(&value)[1]);
		}

		void writeInt(int32_t value) {
			if(m_endianness == Endianness::Little) {
				value = swap(value);
			}

			m_data.push_back(reinterpret_cast<char *>(&value)[0]);
			m_data.push_back(reinterpret_cast<char *>(&value)[1]);
			m_data.push_back(reinterpret_cast<char *>(&value)[2]);
			m_data.push_back(reinterpret_cast<char *>(&value)[3]);
		}

		void writeFloat(float value) {
			if(m_endianness == Endianness::Little) {
				value = swap(value);
			}

			m_data.push_back(reinterpret_cast<char *>(&value)[0]);
			m_data.push_back(reinterpret_cast<char *>(&value)[1]);
			m_data.push_back(reinterpret_cast<char *>(&value)[2]);
			m_data.push_back(reinterpret_cast<char *>(&value)[3]);
		}

		void writeString(const std::string& value) {
			for(int i = 0;i < value.size();i++) {
				m_data.push_back(value[i]);
			}

			m_data.push_back(0);
		}

		BinaryBuffer* construct() {
			BinaryBuffer* buffer = new BinaryBuffer(m_data.size());

			for(int i = 0;i < m_data.size();i++) {
				buffer->Data()[i] = m_data[i];
			}

			return buffer;
		}

		std::vector<char> m_data;
		Endianness m_endianness;
	};

	const static int8_t ObjectIdentifier = 1;
	const static int8_t ArrayIdentifier = 2;
	const static int8_t StringIdentifier = 3;
	const static int8_t Int8Identifier = 4;
	const static int8_t Int16Identifier = 5;
	const static int8_t Int32Identifier = 6;
	const static int8_t FloatIdentifier = 7;
	const static int8_t BoolTrueIdentifier = 8;
	const static int8_t BoolFalseIdentifier = 9;
	const static int8_t NullIdentifier = 10;
	const static int8_t BlobIdentifier = 11;
	const static int8_t CommentIdentifier = 12;
	const static int8_t ContainerEnd = 13;

	void ParseBinaryNode(BinaryReader& reader, Node& node, CommentPolicy commentPolicy) {
		if(node.GetType() == OBJECT_T) {
			while(!reader.eof()) {
				uint32_t temp = reader.pointer();
				char c;
				if(!reader.readChar(c)) {
					std::string errorString = ComposeBinaryError("Unexpected end of data", temp);
				}

				if(c == ContainerEnd) {
					break;
				} else {
					reader.revert();
				}

				uint32_t nameStart = reader.pointer();

				std::string nodeName;
				if(!reader.readString(nodeName)) {
					// Unterminated string
					std::string errorString = ComposeBinaryError("Unterminated name string", nameStart);
					throw Exception(PARSER_ERROR, errorString);
				}

				uint32_t typeStart = reader.pointer();

				char type;
				if(!reader.readChar(type)) {
					// End of data
					std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting type", typeStart);
					throw Exception(PARSER_ERROR, errorString);
				}

				if(type == ObjectIdentifier) {
					Node* newNode = node.CreateObject(nodeName);
					ParseBinaryNode(reader, *newNode, commentPolicy);
				} else if(type == ArrayIdentifier) {
					Node* newNode = node.CreateArray(nodeName);
					ParseBinaryNode(reader, *newNode, commentPolicy);
				} else if(type == StringIdentifier) {
					std::string value;
					temp = reader.pointer();
					if(!reader.readString(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting string data", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateString(value, nodeName);
				} else if(type == Int8Identifier) {
					char value;
					temp = reader.pointer();
					if(!reader.readChar(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was excepting int8 value", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateInt(value, nodeName);
				} else if(type == Int16Identifier) {
					int16_t value;
					temp = reader.pointer();
					if(!reader.readShort(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was excepting int16 value", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateInt(value, nodeName);
				} else if(type == Int32Identifier) {
					int32_t value;
					temp = reader.pointer();
					if(!reader.readInt(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was excepting int32 value", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateInt(value, nodeName);
				} else if(type == FloatIdentifier) {
					float value;
					temp = reader.pointer();
					if(!reader.readFloat(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting float value", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateFloat(value, nodeName);
				} else if(type == BoolTrueIdentifier) {
					node.CreateBool(true, nodeName);
				} else if(type == BoolFalseIdentifier) {
					node.CreateBool(false, nodeName);
				} else if(type == NullIdentifier) {
					node.CreateNull(nodeName);
				} else if(type == BlobIdentifier) {
					int32_t blobSize = 0;
					temp = reader.pointer();
					if(!reader.readInt(blobSize)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting blob size", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					std::vector<uint8_t> blob;
					for(int i = 0;i < blobSize;i++) {
						char value;
						temp = reader.pointer();
						if(!reader.readChar(value)) {
							std::string errorString = ComposeBinaryError("Unexpected end of blob data", temp);
							throw Exception(PARSER_ERROR, errorString);
						}
						blob.push_back(value);
					}
					node.CreateBlob(std::move(blob));
				} else if(type == CommentIdentifier) {
					temp = reader.pointer();
					if(commentPolicy == NO_COMMENTS) {
						std::string errorString = ComposeBinaryError("Comments not supported", temp);
						throw Exception(PARSER_ERROR, errorString);
					} else if(commentPolicy == IGNORE_COMMENTS) {
						std::string value;
						if(!reader.readString(value)) {
							std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting string data", temp);
							throw Exception(PARSER_ERROR, errorString);
						}
					} else {
						std::string value;
						if(!reader.readString(value)) {
							std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting string data", temp);
							throw Exception(PARSER_ERROR, errorString);
						}
						node.CreateComment(value);
					}
				}
			}
		} else if(node.GetType() == ARRAY_T) {
			while(!reader.eof()) {
				uint32_t temp = reader.pointer();
				char c;
				if(!reader.readChar(c)) {
					std::string errorString = ComposeBinaryError("Unexpected end of data", temp);
				}

				if(c == ContainerEnd) {
					break;
				} else {
					reader.revert();
				}

				uint32_t typeStart = reader.pointer();

				char type;
				if(!reader.readChar(type)) {
					// End of data
					std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting type", typeStart);
					throw Exception(PARSER_ERROR, errorString);
				}

				if(type == ObjectIdentifier) {
					Node* newNode = node.CreateObject();
					ParseBinaryNode(reader, *newNode, commentPolicy);
				} else if(type == ArrayIdentifier) {
					Node* newNode = node.CreateArray();
					ParseBinaryNode(reader, *newNode, commentPolicy);
				} else if(type == StringIdentifier) {
					std::string value;
					temp = reader.pointer();
					if(!reader.readString(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting string data", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateString(value);
				} else if(type == Int8Identifier) {
					char value;
					temp = reader.pointer();
					if(!reader.readChar(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was excepting int8 value", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateInt(value);
				} else if(type == Int16Identifier) {
					int16_t value;
					temp = reader.pointer();
					if(!reader.readShort(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was excepting int16 value", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateInt(value);
				} else if(type == Int32Identifier) {
					int32_t value;
					temp = reader.pointer();
					if(!reader.readInt(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was excepting int32 value", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateInt(value);
				} else if(type == FloatIdentifier) {
					float value;
					temp = reader.pointer();
					if(!reader.readFloat(value)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting float value", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					node.CreateFloat(value);
				} else if(type == BoolTrueIdentifier) {
					node.CreateBool(true);
				} else if(type == BoolFalseIdentifier) {
					node.CreateBool(false);
				} else if(type == NullIdentifier) {
					node.CreateNull();
				} else if(type == BlobIdentifier) {
					int32_t blobSize = 0;
					temp = reader.pointer();
					if(!reader.readInt(blobSize)) {
						std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting blob size", temp);
						throw Exception(PARSER_ERROR, errorString);
					}
					std::vector<uint8_t> blob;
					for(int i = 0;i < blobSize;i++) {
						char value;
						temp = reader.pointer();
						if(!reader.readChar(value)) {
							std::string errorString = ComposeBinaryError("Unexpected end of blob data", temp);
							throw Exception(PARSER_ERROR, errorString);
						}
						blob.push_back(value);
					}
					node.CreateBlob(std::move(blob));
				} else if(type == CommentIdentifier) {
					temp = reader.pointer();
					if(commentPolicy == NO_COMMENTS) {
						std::string errorString = ComposeBinaryError("Comments not supported", temp);
						throw Exception(PARSER_ERROR, errorString);
					} else if(commentPolicy == IGNORE_COMMENTS) {
						std::string value;
						if(!reader.readString(value)) {
							std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting string data", temp);
							throw Exception(PARSER_ERROR, errorString);
						}
					} else {
						std::string value;
						if(!reader.readString(value)) {
							std::string errorString = ComposeBinaryError("Unexpected end of data, was expecting string data", temp);
							throw Exception(PARSER_ERROR, errorString);
						}
						node.CreateComment(value);
					}
				}
			}
		}
	}

	Node* ParseBinary(BinaryBuffer& buffer, CommentPolicy commentPolicy) {
		BinaryReader reader(&buffer);
		Node* rootNode = nullptr;

		char firstChar;
		if(!reader.readChar(firstChar)) {
			// Empty buffer
			return nullptr;
		}

		if(firstChar == ObjectIdentifier) {
			rootNode = new Node(OBJECT_T);
		} else if(firstChar == ArrayIdentifier) {
			rootNode = new Node(ARRAY_T);
		} else {
			std::string errorString = ComposeBinaryError("Unexpected type identifier, was expecing object or array", 0);
			throw Exception(PARSER_ERROR, errorString);
		}

		try {
			ParseBinaryNode(reader, *rootNode, commentPolicy);
		} catch(Exception e) {
			if(rootNode != nullptr) {
				delete rootNode;
			}

			throw e;
		}

		return rootNode;
	}

	Node* ParseBinaryFile(const std::string& filename, CommentPolicy commentPolicy) {
		FILE* fp = fopen(filename.c_str(), "r");
		if(fp == nullptr)
		{
			std::string errorString = "Could not read from file '" + filename + "'.\n";
			throw Exception(IO_ERROR, errorString);
		}

		fseek(fp, 0, SEEK_END);
		size_t length = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		char* parseBuffer = new char[length];
		fread(parseBuffer, 1, length, fp);

		fclose(fp);

		BinaryBuffer* buffer = new BinaryBuffer();
		buffer->m_size = length;
		buffer->m_data = parseBuffer;

		Node* rootNode = ParseBinary(*buffer, commentPolicy);

		delete buffer;
		return rootNode;
	}

	void GenerateBinaryNode(BinaryWriter& writer, const Node& node, CommentPolicy commentPolicy) {
		if(node.GetType() == OBJECT_T) {
			writer.writeChar(ObjectIdentifier);
			for(int i = 0;i < node.Children();i++) {
				if(node.GetChild(i)->GetType() != COMMENT_T) {
					writer.writeString(node.GetChild(i)->GetName());
					GenerateBinaryNode(writer, *node.GetChild(i), commentPolicy);
				} else {
					if(commentPolicy == ACCEPT_COMMENTS) {
						writer.writeChar(0);
						GenerateBinaryNode(writer, *node.GetChild(i), commentPolicy);
					}
				}
			}
			writer.writeChar(ContainerEnd);
		} else if(node.GetType() == ARRAY_T) {
			writer.writeChar(ArrayIdentifier);
			for(int i = 0;i < node.Children();i++) {
				if(node.GetChild(i)->GetType() != COMMENT_T) {
					GenerateBinaryNode(writer, *node.GetChild(i), commentPolicy);
				} else {
					if(commentPolicy == ACCEPT_COMMENTS) {
						GenerateBinaryNode(writer, *node.GetChild(i), commentPolicy);
					}
				}
			}
			writer.writeChar(ContainerEnd);
		} else if(node.GetType() == STRING_T) {
			writer.writeChar(StringIdentifier);
			writer.writeString(node.GetString());
		} else if(node.GetType() == INT_T) {
			const static int8_t Int8Max = 0x7f;
			const static int8_t Int8Min = 0x80;
			const static int16_t Int16Max = 0x7fff;
			const static int16_t Int16Min = 0x8000;

			int32_t value = node.GetInt();
			if(value > Int16Max || value < Int16Min) {
				writer.writeChar(Int32Identifier);
				writer.writeInt(value);
			} else if(value > Int8Max || value < Int8Min) {
				writer.writeChar(Int16Identifier);
				writer.writeShort(value);
			} else {
				writer.writeChar(Int8Identifier);
				writer.writeChar(value);
			}
		} else if(node.GetType() == FLOAT_T) {
			writer.writeChar(FloatIdentifier);
			writer.writeFloat(node.GetFloat());
		} else if(node.GetType() == BOOL_T) {
			if(node.GetBool() == true) {
				writer.writeChar(BoolTrueIdentifier);
			} else {
				writer.writeChar(BoolFalseIdentifier);
			}
		} else if(node.GetType() == NULL_T) {
			writer.writeChar(NullIdentifier);
		} else if(node.GetType() == BLOB_T) {
			writer.writeChar(BlobIdentifier);
			std::vector<uint8_t>* blob = node.GetBlob();
			writer.writeInt(blob->size());
			for(int i = 0;i < blob->size();i++) {
				char c = (*blob)[i];
				writer.writeChar(c);
			}
		} else if(node.GetType() == COMMENT_T) {
			writer.writeChar(CommentIdentifier);
			writer.writeString(node.GetComment());
		}
	}

	BinaryBuffer* GenerateBinary(const Node& node, CommentPolicy commentPolicy) {
		BinaryWriter writer;
		GenerateBinaryNode(writer, node, commentPolicy);
		return writer.construct();
	}

	void GenerateBinaryFile(const Node& node, const std::string& filename, CommentPolicy commentPolicy) {
		FILE* fp = fopen(filename.c_str(), "w");
		if(fp == nullptr)
		{
			std::string errorString = "Could not open file '" + filename + "' for writing.\n";
			throw Exception(IO_ERROR, errorString);
		}
		BinaryBuffer* buffer = nullptr;
		try
		{
			buffer = GenerateBinary(node, commentPolicy);
		} catch(Exception e)
		{
			fclose(fp);
			throw e;
		}
		fwrite(buffer->Data(), 1, buffer->Size(), fp);
		fclose(fp);
		delete buffer;
	}
}
