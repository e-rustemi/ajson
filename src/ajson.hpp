#ifndef AJSON_HPP
#define AJSON_HPP

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

/**
 * @mainpage libajson
 * @section intro_section Introduction
 * Ajson is a minimalistic library for parsing and generating json files.\n\n
 * It works on simple principles. Each value in json is represented by a Node.\n
 * Nodes can be of container and primitive types.\n
 * The container types are Objects and Arrays while the primitives are strings, integers, floats, booleans and the null type.\n
 * @section example_section Example
 * @code{.cpp}
 * #include <iostream>
 * #include <ajson.hpp>
 *
 * using namespace std;
 * using namespace ajson;
 *
 * int main() {
 *  // Simple parsing and generating
 * 	try {
 * 		Node* rootNode = ParseJsonFile("clientConfig.json");
 * 		Node* windowSize = rootNode->GetChild("resolution");
 *
 * 		int width, height;
 *
 * 		if(windowSize) {
 * 			width = windowSize->GetChild(0)->GetInt();
 * 			height = windowSize->GetChild(1)->GetInt();
 * 		} else {
 * 			windowSize = rootNode->CreateChild(ARRAY_T, "resolution");
 *
 * 			Node* widthNode = windowSize->CreateInt(640);
 * 			Node* heightNode = windowSize->CreateInt(480);
 * 		}
 *
 * 		Node* fullscreenNode = rootNode->GetChild("fullscreen");
 * 		if(!fullscreenNode) {
 * 			fullscreenNode = rootNode->CreateBool(false, "fullscreen");
 * 		}
 *
 * 		bool fullscreen = fullscreenNode->GetBool();
 *
 * 		GenerateJsonFile(*rootNode, "clientConfig.json");
 * 	} catch(Exception e) {
 * 		cout << e.what();
 * 	}
 *
 * 	return 0;
 * }
 * @endcode
 */

namespace ajson
{
	/**
	 * @brief Output type of generateJson() and generateJsonFile().
	 */
	enum JsonOutput : char
	{
		/**
		 * @brief All nodes are next to each other with no whitespace in between.
		 */
		JSON_COMPACT,
		/**
		 * @brief Each node has its own line and for every depth level there is a tab.
		 */
		JSON_SPACED
	};

	/**
	 * @brief Error codes of ajson library.
	 */
	enum ErrorCode : char
	{
		/**
		 * @brief PARSER_ERROR is thrown when it encounters incorrect syntax. Comments not supported.
		 */
		PARSER_ERROR,
		/**
		 * @brief AST_ERROR is thrown when trying to add child nodes to non-container types such as boolean or strings.
		 */
		AST_ERROR,
		/**
		 * @brief IO_ERROR is thrown when ajson could not open file for writing or reading.
		 */
		IO_ERROR
	};

	enum CommentPolicy : char
	{
		/**
		 * @brief Throw PARSER_ERROR during parse, does not print comments when generating.
		 */
		NO_COMMENTS,
		/**
		 * @brief Parse comments but don't add them as nodes, does not print comments when generating.
		 */
		IGNORE_COMMENTS,
		/**
		 * @brief Parse comments and add as nodes, comments are printed when generating.
		 */
		ACCEPT_COMMENTS
	};

	union Value
	{
		bool					boolValue;
		char					charValue;
		std::string*			stringValue;
		std::vector<uint8_t>*	blobValue;
		int32_t					intValue;
		float					floatValue;
	};

	/**
	 * @brief Type of node.
	 */
	enum NodeType : char
	{
		OBJECT_T,
		ARRAY_T,
		STRING_T,
		INT_T,
		FLOAT_T,
		BOOL_T,
		NULL_T,
		BLOB_T,

		COMMENT_T
	};

	class Node;
	class NodeIterator;

	class Exception
	{
	public:
		Exception(enum ErrorCode error) : m_error{error} {};
		Exception(enum ErrorCode error, std::string errorString) : m_error{error}, m_errorString{errorString} {};
		~Exception() {};

		inline std::string what() const {
			return m_errorString;
		};

		Node* GetNode() const {
			return m_node;
		}

		ErrorCode GetErrorCode() const {
			return m_error;
		}

		enum ErrorCode 		m_error;
		std::string 		m_errorString;
		Node*				m_node;
	};

	class Node final
	{
	public:
		explicit Node(enum NodeType type = NULL_T, const std::string& name = "");

		/**
		 * Int type ctor
		 */
		explicit Node(int32_t intValue, const std::string& name = "");

		/**
		 * Float type ctor
		 */
		explicit Node(float floatValue, const std::string& name = "");

		/**
		 * Bool type ctor
		 */
		explicit Node(bool boolValue, const std::string& name = "");

		/**
		 * String type ctor
		 */
		explicit Node(const std::string& stringValue, const std::string& name = "");

		/**
		 * Blob type copy values ctor
		 */
		explicit Node(const std::vector<uint8_t>& blobValue, const std::string& name = "");

		/**
		 * Blob type move values ctor
		 */
		explicit Node(std::vector<uint8_t>&& blobValue, const std::string& name = "");

		Node(Node&& node);

		void operator=(const Node& node);
		void operator=(Node&& node);

		~Node();

		/**
		 * @brief Gets type of the node.
		 */
		inline enum NodeType GetType() const {
			return m_type;
		}
		/**
		 * @brief Sets type of node.
		 * If node is of container type it's children are deleted recursively.
		 */
		void 		SetType(enum NodeType t);

		inline bool IsObject() const {
			if(m_type == OBJECT_T)
				return true;
			else
				return false;
		};

		inline bool	IsArray() const {
			if(m_type == ARRAY_T)
				return true;
			else
				return false;
		};

		inline bool	IsString() const {
			if(m_type == STRING_T)
				return true;
			else
				return false;
		};

		inline bool	IsInt() const {
			if(m_type == INT_T)
				return true;
			else
				return false;
		};

		inline bool	IsFloat() const {
			if(m_type == FLOAT_T)
				return true;
			else
				return false;
		};

		inline bool IsNumber() const {
			if(m_type == INT_T || m_type == FLOAT_T)
				return true;
			else
				return false;
		}

		inline bool IsBool() const {
			if(m_type == BOOL_T)
				return true;
			else
				return false;
		};

		inline bool	IsNull() const {
			if(m_type == NULL_T)
				return true;
			else
				return false;
		}

		inline bool IsComment() const {
			if(m_type == COMMENT_T)
				return true;
			else
				return false;
		}

		inline bool IsBlob() const {
			if(m_type == BLOB_T) {
				return true;
			} else {
				return false;
			}
		}

		/**
		 * @brief Gets name of the node.
		 */
		std::string	GetName() const {
			return m_name;
		}
		/**
		 * @brief Sets node name.
		 */
		void     	SetName(const std::string& name);

		/**
		 * @brief Get the boolean value.
		 * Undefined behaviour if node type is not BOOL_T.
		 */
		bool     	GetBool() const;
		/**
		 * @brief Set the node type to bool and set bool value.
		 * Deletes all children recursively if node is a container type.
		 */
		void     	SetBool(bool val);

		/**
		 * @brief Get the string value.
		 */
		std::string	GetString() const;
		/**
		 * @brief Set the node type to string and set string value.
		 * Deletes all children if node is a container type.
		 */
		void		SetString(const std::string& val);

		/**
		 * @brief Get comment text
		 */
		std::string GetComment() const;

		/**
		 * @brief Set the node type to comment and set it's text.
		 * Deletes all children if node is a container type.
		 */
		void 		SetComment(const std::string& val);

		/**
		 * @brief Get the int value.
		 */
		int32_t		GetInt() const;
		/**
		 * @brief Set the node type to int and set int value.
		 * Deletes all children recursively if node is a container type.
		 */
		void		SetInt(int32_t val);

		/**
		 * @brief Get the float value.
		 */
		float		GetFloat() const;
		/**
		 * @brief Set the node type to float and set float value.
		 * Deletes all children recursively if node is a container type.
		 */
		void		SetFloat(float val);

		/**
		 * @brief Get blob value.
		 * Returns empty vector if node is not blob type.
		 */
		std::vector<uint8_t>* GetBlob() const;

		/**
		 * @brief Set the node type to blob and set blob value with data copying.
		 * Deletes all children recursively if node is a container type.
		 */
		void SetBlob(const std::vector<uint8_t>& val);

		/**
		 * @brief Set the node type to blob and move data to blob value.
		 * Deletes all children recursively if node is a container type.
		 */
		void SetBlob(std::vector<uint8_t>&& val);


		typedef NodeIterator iterator;

		iterator 	begin() const;
		iterator 	end() const;

		/**
		 * @brief Number of children.
		 */
		uint16_t 	Children() const;
		/**
		 * @brief Get the nth child of node.
		 * Returns nullptr if out of bounds.
		 * Returns nullptr if node is not container type.
		 */
		Node*		GetChild(uint16_t n) const;
		/**
		 * @brief Search child nodes in object node by name and returns last occurrence of name or nullptr if not found.
		 * Returns nullptr if node is not object type.
		 */
		Node*		GetChild(const std::string& name) const;

		Node*		GetFirstChild() const;
		Node*		GetLastChild() const;
		/**
		 * @brief Get parent node.
		 * Returns nullptr only if this is the root node.
		 */
		Node*		GetParent() const;

		/**
		 * @brief Add child. Node is left moved from state. Use returned pointer instead.
		 */
		Node*		AddChild(Node* node);
		/**
		 * @brief Add child at specific location. Node is left moved from state. Use returned pointer instead.
		 */
		Node*		AddChild(Node* node, uint16_t index);
		/**
		 * @brief Removes the nth child.
		 * No effect if the nth child doesn't exist.
		 */
		void		RemoveChild(uint16_t n);
		void		RemoveChild(Node* node);

		void		RemoveAllChildren();

		Node* 		CreateChild(enum NodeType t, const std::string& name = "");

		Node* 		CreateObject(const std::string& name = "");
		Node* 		CreateArray(const std::string& name = "");
		Node* 		CreateString(const std::string& value, const std::string& name = "");
		Node* 		CreateInt(int32_t value, const std::string& name = "");
		Node*		CreateFloat(float value, const std::string& name = "");
		Node* 		CreateBool(bool value, const std::string& name = "");
		Node* 		CreateNull(const std::string& name = "");

		/**
		 * Create blob child node with value copy.
		 */
		Node* 		CreateBlob(const std::vector<uint8_t>& value, const std::string& name = "");

		/**
		 * Create blob child node moving the blob value.
		 */
		 Node*		CreateBlob(std::vector<uint8_t>&& value, const std::string& name = "");

		Node*		CreateComment(const std::string& value);

		/**
		 * @brief Returns the nesting level of a node.
		 * E.g. The root node has a depth of 0 and its children a depth of 1, children of its children a depth of 2 and so on.
		 */
		uint16_t	GetDepth() const;
	private:
		Node*				m_parent;
		std::vector<Node>	m_children;

		std::string			m_name;

		enum NodeType		m_type;
		union Value			m_value;
	};

	/**
	 * @brief Convenience class for iterating over nodes.
	 */
	class NodeIterator
	{
		friend class Node;
	public:
		inline NodeIterator& operator++()
		{
			m_child++;
			return *this;
		}
		inline NodeIterator operator++(int)
		{
			NodeIterator i;
			i.m_child = m_child;
			i.m_node = m_node;
			m_child++;
			return i;
		}
		inline NodeIterator& operator--()
		{
			m_child--;
			return *this;
		}
		inline NodeIterator operator--(int)
		{
			NodeIterator i;
			i.m_child = m_child;
			i.m_node = m_node;
			m_child--;
			return i;
		}
		inline NodeIterator& operator+(int n)
		{
			m_child += n;
			return *this;
		}
		inline NodeIterator& operator-(int n)
		{
			m_child -= n;
			return *this;
		}
		inline void operator+=(int n)
		{
			m_child += n;
		}
		inline void operator-=(int n)
		{
			m_child -= n;
		}
		inline Node* operator*() const
		{
			return m_node->GetChild(m_child);
		}
		inline Node* operator->() const
		{
			return m_node->GetChild(m_child);
		}
		inline bool operator!=(NodeIterator& it) const
		{
			if(m_child != it.m_child)
				return true;
			else
				return false;
		}
		inline bool operator!=(NodeIterator&& it) const
		{
			if(m_child != it.m_child)
				return true;
			else
				return false;
		}
		inline bool operator==(NodeIterator& it) const
		{
			if(m_child == it.m_child)
				return true;
			else
				return false;
		}
		inline bool operator==(NodeIterator&& it) const
		{
			if(m_child == it.m_child)
				return true;
			else
				return false;
		}
	private:
		Node* m_node;
		uint32_t m_child;
	};

	struct BinaryBuffer {
		BinaryBuffer() {
			m_size = 0;
			m_data = nullptr;
		}

		BinaryBuffer(uint32_t sz) {
			m_size = sz;
			m_data = new char[sz];
		}

		~BinaryBuffer() {
			m_size = 0;
			if(m_data) {
				delete[] m_data;
				m_data = nullptr;
			}
		}

		uint32_t Size() const {
			return m_size;
		}

		char* Data() {
			return m_data;
		}

		uint32_t m_size;
		char* m_data;
	};

	BinaryBuffer* 	GenerateBinary(const Node& node, CommentPolicy commentPolicy = IGNORE_COMMENTS);
	void 			GenerateBinaryFile(const Node& node, const std::string& filename, CommentPolicy commentPolicy = IGNORE_COMMENTS);

	Node*			ParseBinary(BinaryBuffer& buffer, CommentPolicy commentPolicy = IGNORE_COMMENTS);
	Node* 			ParseBinaryFile(const std::string& filename, CommentPolicy commentPolicy = IGNORE_COMMENTS);

	template<enum JsonOutput formatStyle = JSON_SPACED>
		   		void GenerateJsonFile(const Node& node, const std::string& filename, CommentPolicy commentPolicy = IGNORE_COMMENTS);
	/**
	 * @brief Create file in spaced format.
	 * Throws an exception if it can't open the specified file for writing.
	 *
	 * @sa JsonOutput
	 */
	template<> 	void GenerateJsonFile<JSON_SPACED>(const Node& node, const std::string& filename, CommentPolicy commentPolicy);
	/**
	 * @brief Create file in compact format.
	 * Throws an exception if it can't open the specified file for writing.
	 *
	 * @sa JsonOutput
	 */
	template<> 	void GenerateJsonFile<JSON_COMPACT>(const Node& node, const std::string& filename, CommentPolicy commentPolicy);

	template<enum JsonOutput formatStyle = JSON_SPACED>
				std::string GenerateJson(const Node& node, CommentPolicy commentPolicy = IGNORE_COMMENTS);

	/**
	 * @brief Output with no whitespace.
	 */
	template<>	std::string GenerateJson<JSON_COMPACT>(const Node& node, CommentPolicy commentPolicy);
	/**
	 * @brief Output is properly indented.
	 */
	template<>	std::string GenerateJson<JSON_SPACED>(const Node& node, CommentPolicy commentPolicy);

	/**
	 * @brief Parse json from buffer.
	 *
	 * @sa parseJsonFile
	 **/
	Node*		ParseJson(const std::string& parseBuffer, CommentPolicy commentPolicy = IGNORE_COMMENTS);

	/**
	 * @brief Parse json from file.
	 * Throws exception if it can't open specified file for reading.
	 */
	Node*	 	ParseJsonFile(const std::string& filename, CommentPolicy commentPolicy = IGNORE_COMMENTS);
};

#endif //ajson.hpp
