#include <iostream>
#include <string>
#include <fstream>
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

    Node* parseRegex() {
        Node* node = parseConcat();
        while (current() == '+') {
            consume();
            Node* right = parseConcat();
            node = new Node('+', node, right);
        }
        return node;
    }

    Node* parseConcat() {
        Node* node = parseStar();
        while (isAtomStart(current())) {
            Node* right = parseStar();
            node = new Node('.', node, right);
        }
        return node;
    }

    Node* parseStar() {
        Node* node = parseAtom();
        while (current() == '*') {
            consume();
            node = new Node('*', node, nullptr);
        }
        return node;
    }

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

// ── Tree visualization ────────────────────────────────────────────────────────

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

// ── List/tuple serialization ──────────────────────────────────────────────────

string serializeList(Node* node) {
    if (!node) return "";
    if (!node->left && !node->right) return string(1, node->value);
    if (node->left && node->right)
        return "[" + string(1, node->value) + ", "
               + serializeList(node->left) + ", "
               + serializeList(node->right) + "]";
    return "[" + string(1, node->value) + ", " + serializeList(node->left) + "]";
}

// ── Size metrics ──────────────────────────────────────────────────────────────

int sizeAll(Node* n) {
    if (!n) return 0;
    return 1 + sizeAll(n->left) + sizeAll(n->right);
}

int sizeLeaves(Node* n) {
    if (!n) return 0;
    if (!n->left && !n->right) return 1;
    return sizeLeaves(n->left) + sizeLeaves(n->right);
}

// ── Thompson NFA state / arc count ───────────────────────────────────────────
// Construction rules (Dragon Book / Thompson 1968):
//   symbol a  : 2 states,  1 arc
//   r1 + r2   : +2 states, +4 epsilon arcs  (new start/accept)
//   r1 . r2   : -1 state,  +0 arcs          (merge accept(r1) with start(r2))
//   r*        : +2 states, +4 epsilon arcs  (new start/accept + back-edge)
//
// Upper bounds based on total node count n = sizeAll:
//   states <= 2n,  arcs <= 4n

struct NFA { int states, arcs; };

NFA thompsonNFA(Node* n) {
    if (!n) return {0, 0};

    if (!n->left && !n->right) return {2, 1};   // leaf

    if (n->left && !n->right) {                  // star (unary)
        NFA c = thompsonNFA(n->left);
        return {c.states + 2, c.arcs + 4};
    }

    NFA l = thompsonNFA(n->left);
    NFA r = thompsonNFA(n->right);

    if (n->value == '+')                         // union
        return {l.states + r.states + 2, l.arcs + r.arcs + 4};

    return {l.states + r.states - 1, l.arcs + r.arcs};  // concat
}

// ── JSON serialization for D3 ─────────────────────────────────────────────────

string toJSON(Node* n) {
    if (!n) return "null";
    string name(1, n->value);
    if (!n->left && !n->right)
        return "{\"name\":\"" + name + "\"}";
    string s = "{\"name\":\"" + name + "\",\"children\":[";
    if (n->left && n->right)
        s += toJSON(n->left) + "," + toJSON(n->right);
    else if (n->left)
        s += toJSON(n->left);
    s += "]}";
    return s;
}

// ── D3.js HTML generation ─────────────────────────────────────────────────────

void generateHTML(Node* root, const string& regex,
                  int sz, int leaves, int ops, const NFA& exact) {
    ofstream f("hw2_tree.html");
    if (!f) { cerr << "  [오류] HTML 파일을 열 수 없습니다.\n"; return; }

    string json = toJSON(root);

    f << "<!DOCTYPE html>\n"
         "<html lang=\"ko\">\n"
         "<head>\n"
         "<meta charset=\"UTF-8\">\n"
         "<title>Regex Tree</title>\n"
         "<script src=\"https://d3js.org/d3.v7.min.js\"></script>\n"
         "<style>\n"
         "  body{margin:0;background:#1e1e2e;color:#cdd6f4;font-family:monospace}\n"
         "  h2{text-align:center;padding:16px 0 4px;font-size:1.2em}\n"
         "  .info{text-align:center;font-size:.88em;color:#a6adc8;padding-bottom:14px}\n"
         "  .operator circle{fill:#89b4fa;stroke:#1e1e2e;stroke-width:2.5px}\n"
         "  .leaf     circle{fill:#a6e3a1;stroke:#1e1e2e;stroke-width:2.5px}\n"
         "  .node text{font-size:14px;font-weight:bold;fill:#1e1e2e;pointer-events:none}\n"
         "  .link{fill:none;stroke:#585b70;stroke-width:1.8px}\n"
         "  .legend{position:absolute;top:12px;right:20px;font-size:.8em;color:#a6adc8}\n"
         "  .legend span{display:inline-block;width:12px;height:12px;"
         "border-radius:50%;margin-right:4px;vertical-align:middle}\n"
         "  .op-dot{background:#89b4fa} .leaf-dot{background:#a6e3a1}\n"
         "</style>\n"
         "</head>\n"
         "<body>\n";

    f << "<h2>정규표현 트리 &nbsp; <code>" << regex << "</code></h2>\n";
    f << "<div class=\"info\">"
      << "size = " << sz
      << " &nbsp;|&nbsp; 단말 노드 = " << leaves
      << " &nbsp;|&nbsp; 연산자 노드 = " << ops
      << " &nbsp;|&nbsp; NFA 상태 = " << exact.states
      << " &nbsp;|&nbsp; NFA 전이 = " << exact.arcs
      << "</div>\n";
    f << "<div class=\"legend\">"
         "<span class=\"op-dot\"></span>연산자 &nbsp;"
         "<span class=\"leaf-dot\"></span>단말"
         "</div>\n";
    f << "<svg id=\"svg\"></svg>\n";
    f << "<script>\n";
    f << "const data = " << json << ";\n";
    f << "const margin = {top:50, right:180, bottom:50, left:80};\n"
         "const W = Math.max(960, window.innerWidth)  - margin.left - margin.right;\n"
         "const H = Math.max(500, window.innerHeight) - margin.top  - margin.bottom;\n"
         "const svg = d3.select('#svg')\n"
         "  .attr('width',  W + margin.left + margin.right)\n"
         "  .attr('height', H + margin.top  + margin.bottom)\n"
         "  .append('g').attr('transform', `translate(${margin.left},${margin.top})`);\n"
         "const root = d3.hierarchy(data);\n"
         "d3.tree().size([H, W])(root);\n"
         "svg.selectAll('.link').data(root.links()).enter().append('path')\n"
         "  .attr('class','link')\n"
         "  .attr('d', d3.linkHorizontal().x(d=>d.y).y(d=>d.x));\n"
         "const node = svg.selectAll('.node').data(root.descendants())\n"
         "  .enter().append('g')\n"
         "  .attr('class', d => 'node ' + (d.children ? 'operator' : 'leaf'))\n"
         "  .attr('transform', d => `translate(${d.y},${d.x})`);\n"
         "node.append('circle').attr('r', 18);\n"
         "node.append('text').attr('dy','0.35em').attr('text-anchor','middle')\n"
         "  .text(d => d.data.name);\n";
    f << "</script>\n</body>\n</html>\n";

    f.close();
    cout << "  -> hw2_tree.html 생성됨 (브라우저로 열어 확인하세요)\n";
}

// ── Memory management ─────────────────────────────────────────────────────────

void deleteTree(Node* node) {
    if (!node) return;
    deleteTree(node->left);
    deleteTree(node->right);
    delete node;
}

// ── Main ──────────────────────────────────────────────────────────────────────

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

    // ① 트리 구조
    cout << "\n[트리 구조]\n";
    cout << "  연산자: + 합집합  . 접속  * 클리니스타  |  단말: 알파벳.숫자\n\n";
    printTreeRoot(root);

    // ② 리스트 표현
    cout << "\n[리스트 표현]\n";
    cout << "  " << serializeList(root) << "\n";

    // ③ 크기 정보
    int sz       = sizeAll(root);
    int leaves   = sizeLeaves(root);
    int ops      = sz - leaves;
    cout << "\n[크기 정보]\n";
    cout << "  전체 노드 수 (size) : " << sz      << "\n";
    cout << "  단말 노드 수 (잎)   : " << leaves  << "\n";
    cout << "  연산자 노드 수      : " << ops     << "\n";

    // ④ Thompson NFA 상태·전이 수
    NFA exact   = thompsonNFA(root);
    int stateUB = 2 * sz;
    int arcUB   = 4 * sz;
    cout << "\n[Thompson NFA 상태 및 전이(arc) 수]\n";
    cout << "  상한 (2n / 4n 공식) : 상태 <= " << stateUB
         << ",  전이 <= " << arcUB << "  (n = size = " << sz << ")\n";
    cout << "  정확한 수           : 상태  = " << exact.states
         << ",  전이  = " << exact.arcs << "\n";

    // ⑤ D3.js HTML 시각화 파일 생성
    cout << "\n[D3.js 시각화]\n";
    generateHTML(root, regex, sz, leaves, ops, exact);

    deleteTree(root);
    return 0;
}
