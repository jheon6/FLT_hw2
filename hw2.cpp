#include <iostream>
#include <string>
using namespace std;

struct Node {
    char value;
    Node* left;
    Node* right;

    Node(char v, Node* l = nullptr, Node* r = nullptr)
        : value(v), left(l), right(r) {}
};

// Grammar (higher priority operator -> deeper in tree):
//   regex  -> concat ('+' concat)*      lowest priority
//   concat -> star   (star)*            middle priority
//   star   -> atom   ('*')*             highest priority (postfix)
//   atom   -> CHAR | '(' regex ')'
class RegexParser {
    string input;
    int pos;

public:
    RegexParser(const string& s) : input(s), pos(0) {}

    Node* parse() {
        if (input.empty()) return nullptr;
        return parseRegex();
    }

private:
    char current() const {
        return pos < (int)input.size() ? input[pos] : '\0';
    }

    void consume() { pos++; }

    bool isAtomStart(char c) const {
        return isalnum((unsigned char)c) || c == '(';
    }

    // '+' union: lowest priority, left-associative
    Node* parseRegex() {
        Node* node = parseConcat();
        while (current() == '+') {
            consume();
            Node* right = parseConcat();
            node = new Node('+', node, right);
        }
        return node;
    }

    // concatenation: middle priority, left-associative (represented as '.')
    Node* parseConcat() {
        Node* node = parseStar();
        while (isAtomStart(current())) {
            Node* right = parseStar();
            node = new Node('.', node, right);
        }
        return node;
    }

    // '*' Kleene star: highest priority, postfix unary
    Node* parseStar() {
        Node* node = parseAtom();
        while (current() == '*') {
            consume();
            node = new Node('*', node, nullptr);
        }
        return node;
    }

    // atom: single symbol or parenthesized sub-expression
    Node* parseAtom() {
        if (current() == '(') {
            consume();
            Node* node = parseRegex();
            if (current() == ')') consume();
            return node;
        }
        char c = current();
        consume();
        return new Node(c);
    }
};

// Tree visualization
void printTree(Node* node, const string& prefix, bool isLast) {
    if (!node) return;

    cout << prefix << (isLast ? "L-- " : "|-- ") << node->value << "\n";

    string childPrefix = prefix + (isLast ? "    " : "|   ");

    if (node->left && node->right) {
        printTree(node->left,  childPrefix, false);
        printTree(node->right, childPrefix, true);
    } else if (node->left) {
        printTree(node->left, childPrefix, true);
    }
}

void printTreeRoot(Node* node) {
    if (!node) return;
    cout << node->value << "\n";
    if (node->left && node->right) {
        printTree(node->left,  "", false);
        printTree(node->right, "", true);
    } else if (node->left) {
        printTree(node->left, "", true);
    }
}

// List/tuple serialization
// Leaf   -> single character
// Unary  -> [*, child]
// Binary -> [op, left, right]
string serializeList(Node* node) {
    if (!node) return "";

    bool isLeaf   = !node->left && !node->right;
    bool isBinary =  node->left &&  node->right;

    if (isLeaf)
        return string(1, node->value);

    if (isBinary)
        return string("[") + node->value + ", "
               + serializeList(node->left) + ", "
               + serializeList(node->right) + "]";

    // unary (*)
    return string("[") + node->value + ", " + serializeList(node->left) + "]";
}

void deleteTree(Node* node) {
    if (!node) return;
    deleteTree(node->left);
    deleteTree(node->right);
    delete node;
}

int main() {
    string regex;
    cout << "정규표현 입력: ";
    cin >> regex;

    RegexParser parser(regex);
    Node* root = parser.parse();

    if (!root) {
        cout << "입력이 비어있습니다.\n";
        return 0;
    }

    cout << "\n[트리 구조]\n";
    cout << "  연산자: + 합집합  . 접속  * 클리니스타  |  단말: 알파벳.숫자\n\n";
    printTreeRoot(root);

    cout << "\n[리스트 표현]\n";
    cout << "  " << serializeList(root) << "\n";

    deleteTree(root);
    return 0;
}
