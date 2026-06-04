#include <iostream>
#include <string>
#include <fstream>
#include <stdexcept>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// 1단계: 파스 트리
// ─────────────────────────────────────────────────────────────────────────────

struct Node {
    char value;
    Node* left;
    Node* right;
    Node(char v, Node* l = nullptr, Node* r = nullptr)
        : value(v), left(l), right(r) {}
};

void deleteTree(Node* node);

// Grammar:
//   regex  -> concat ('+' concat)*
//   concat -> star   (star)*
//   star   -> atom   ('*')*
//   atom   -> CHAR | '(' regex ')'
class RegexParser {
    string input;
    int pos;
public:
    RegexParser(const string& s) : input(s), pos(0) {}

    Node* parse() {
        if (input.empty()) return nullptr;
        Node* root = parseRegex();
        if (pos < (int)input.size()) {
            deleteTree(root);
            throw runtime_error(
                string("예상치 못한 문자 '") + input[pos] +
                "' (위치 " + to_string(pos + 1) + ")");
        }
        return root;
    }

private:
    char current() const { return pos < (int)input.size() ? input[pos] : '\0'; }
    void consume() { pos++; }
    bool isAtomStart(char c) const { return isalnum((unsigned char)c) || c == '('; }

    Node* parseRegex() {
        Node* node = parseConcat();
        while (current() == '+') {
            consume();
            if (!isAtomStart(current())) {
                deleteTree(node);
                throw runtime_error(
                    current() == '\0'
                        ? string("'+' 뒤에 피연산자가 없습니다")
                        : string("'+' 뒤에 잘못된 문자 '") + current() + "'");
            }
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
            if (current() != ')') {
                deleteTree(node);
                throw runtime_error("닫는 괄호 ')' 가 없습니다");
            }
            consume();
            return node;
        }
        if (current() == '\0')
            throw runtime_error("피연산자가 필요한 위치에서 입력이 끝났습니다");
        if (!isalnum((unsigned char)current()))
            throw runtime_error(
                string("잘못된 문자 '") + current() +
                "' (위치 " + to_string(pos + 1) + ")");
        char c = current();
        consume();
        return new Node(c);
    }
};

// ── 트리 출력 ─────────────────────────────────────────────────────────────────

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

string serializeList(Node* node) {
    if (!node) return "";
    if (!node->left && !node->right) return string(1, node->value);
    if (node->left && node->right)
        return "[" + string(1, node->value) + ", "
               + serializeList(node->left) + ", "
               + serializeList(node->right) + "]";
    return "[" + string(1, node->value) + ", " + serializeList(node->left) + "]";
}

int sizeAll(Node* n) {
    if (!n) return 0;
    return 1 + sizeAll(n->left) + sizeAll(n->right);
}

int sizeLeaves(Node* n) {
    if (!n) return 0;
    if (!n->left && !n->right) return 1;
    return sizeLeaves(n->left) + sizeLeaves(n->right);
}

// ── Thompson NFA 상태·전이 수 추정 ────────────────────────────────────────────

struct NFACount { int states, arcs; };

NFACount thompsonCount(Node* n) {
    if (!n) return {0, 0};
    if (!n->left && !n->right) return {2, 1};
    if (n->left && !n->right) {
        NFACount c = thompsonCount(n->left);
        return {c.states + 2, c.arcs + 4};
    }
    NFACount l = thompsonCount(n->left);
    NFACount r = thompsonCount(n->right);
    if (n->value == '+')
        return {l.states + r.states + 2, l.arcs + r.arcs + 4};
    return {l.states + r.states, l.arcs + r.arcs + 1};
}

// ── JSON / HTML 이스케이프 ─────────────────────────────────────────────────────

string jsonEscape(char c) {
    if (c == '"')  return "\\\"";
    if (c == '\\') return "\\\\";
    return string(1, c);
}

string htmlEscape(const string& s) {
    string out;
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;
        }
    }
    return out;
}

string toJSON(Node* n) {
    if (!n) return "null";
    string name = jsonEscape(n->value);
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

void deleteTree(Node* node) {
    if (!node) return;
    deleteTree(node->left);
    deleteTree(node->right);
    delete node;
}

// ─────────────────────────────────────────────────────────────────────────────
// 2단계: Thompson ε-NFA 구성
// ─────────────────────────────────────────────────────────────────────────────

const char EPS = '\0';   // ε 전이를 나타내는 내부 기호

struct NFAFrag { int start, accept; };

class ThompsonNFA {
public:
    int numStates  = 0;
    int startState = 0;
    int acceptState = 0;
    set<char> alphabet;
    // delta[상태][입력심볼] = {다음 상태 집합}
    map<int, map<char, set<int>>> delta;

    int newState() { return numStates++; }

    void addTrans(int from, char sym, int to) {
        delta[from][sym].insert(to);
        if (sym != EPS) alphabet.insert(sym);
    }

    // Thompson 구성법 — 재귀적으로 NFA 프래그먼트를 생성
    NFAFrag buildFrag(Node* node) {
        if (!node->left && !node->right) {
            // 단말(심볼): s --(sym)--> a
            int s = newState(), a = newState();
            addTrans(s, node->value, a);
            return {s, a};
        }
        if (node->value == '*') {
            // 클리니 스타: 새 시작/종결 + 루프 ε 전이
            NFAFrag c = buildFrag(node->left);
            int s = newState(), a = newState();
            addTrans(s, EPS, c.start); addTrans(s, EPS, a);
            addTrans(c.accept, EPS, c.start); addTrans(c.accept, EPS, a);
            return {s, a};
        }
        if (node->value == '+') {
            // 합집합(union): 새 시작/종결 + 각 브랜치로 ε 전이
            NFAFrag l = buildFrag(node->left);
            NFAFrag r = buildFrag(node->right);
            int s = newState(), a = newState();
            addTrans(s, EPS, l.start); addTrans(s, EPS, r.start);
            addTrans(l.accept, EPS, a); addTrans(r.accept, EPS, a);
            return {s, a};
        }
        // 접속(concat): l.accept --(ε)--> r.start
        NFAFrag l = buildFrag(node->left);
        NFAFrag r = buildFrag(node->right);
        addTrans(l.accept, EPS, r.start);
        return {l.start, r.accept};
    }

    void construct(Node* root) {
        NFAFrag f = buildFrag(root);
        startState  = f.start;
        acceptState = f.accept;
    }
};

// ── ε-NFA 구성 요소 콘솔 출력 ─────────────────────────────────────────────────

void printNFAComponents(const ThompsonNFA& nfa) {
    // 상태 집합
    cout << "  상태 집합 Q      : {";
    for (int i = 0; i < nfa.numStates; i++) {
        if (i) cout << ", ";
        cout << "q" << i;
    }
    cout << "}\n";

    // 입력 심볼 집합
    cout << "  입력 심볼 집합 Σ : {";
    bool first = true;
    for (char c : nfa.alphabet) { if (!first) cout << ", "; cout << c; first = false; }
    cout << "}\n";

    cout << "  시작 상태        : q" << nfa.startState << "\n";
    cout << "  종결 상태 집합 F : {q" << nfa.acceptState << "}\n";

    int cnt = 0;
    for (auto& [s, m] : nfa.delta)
        for (auto& [sym, ts] : m) cnt += (int)ts.size();
    cout << "  전이 수          : " << cnt << "\n";
}

// ── 전이 함수 테이블 콘솔 출력 ────────────────────────────────────────────────

void printTransitionTable(const ThompsonNFA& nfa) {
    vector<char> cols(nfa.alphabet.begin(), nfa.alphabet.end());
    cols.push_back(EPS);   // ε 열을 맨 오른쪽에 배치

    // 각 셀 문자열 생성
    auto cellStr = [&](int s, char c) -> string {
        auto sit = nfa.delta.find(s);
        if (sit == nfa.delta.end()) return "-";
        auto cit = sit->second.find(c);
        if (cit == sit->second.end() || cit->second.empty()) return "-";
        string r = "{";
        bool f = true;
        for (int t : cit->second) { if (!f) r += ","; r += "q" + to_string(t); f = false; }
        return r + "}";
    };

    auto colLabel = [](char c) -> string { return (c == EPS) ? "e" : string(1, c); };

    // 열 너비 계산
    vector<int> colW;
    for (char c : cols) {
        int w = (int)colLabel(c).size();
        for (int s = 0; s < nfa.numStates; s++)
            w = max(w, (int)cellStr(s, c).size());
        colW.push_back(w + 2);
    }

    auto center = [](const string& s, int w) -> string {
        int pad = w - (int)s.size();
        int lp = pad / 2;
        return string(lp, ' ') + s + string(pad - lp, ' ');
    };

    const int sW = 7;   // 상태 레이블 너비

    // 헤더
    cout << "  " << string(sW, ' ') << " |";
    for (int i = 0; i < (int)cols.size(); i++)
        cout << center(colLabel(cols[i]), colW[i]) << "|";
    cout << "\n";

    // 구분선
    cout << "  " << string(sW, '-') << "-+";
    for (int w : colW) cout << string(w, '-') << "+";
    cout << "\n";

    // 행
    for (int s = 0; s < nfa.numStates; s++) {
        bool isS = (s == nfa.startState), isA = (s == nfa.acceptState);
        string sl;
        if      (isS && isA) sl = "->*q" + to_string(s);
        else if (isS)        sl = " ->q" + to_string(s);
        else if (isA)        sl = "  *q" + to_string(s);
        else                 sl = "   q" + to_string(s);
        int pad = sW - (int)sl.size();
        cout << "  " << string(max(0, pad), ' ') << sl << " |";
        for (int i = 0; i < (int)cols.size(); i++)
            cout << center(cellStr(s, cols[i]), colW[i]) << "|";
        cout << "\n";
    }
    cout << "  (-> : 시작 상태,  * : 종결 상태,  e 열 : epsilon 전이)\n";
}

// ── NFA → JSON (HTML 시각화용) ────────────────────────────────────────────────

string nfaToJSON(const ThompsonNFA& nfa) {
    string s = "{\"states\":[";
    for (int i = 0; i < nfa.numStates; i++) {
        if (i) s += ",";
        s += "{\"id\":" + to_string(i)
           + ",\"isStart\":"  + (i == nfa.startState  ? "true" : "false")
           + ",\"isAccept\":" + (i == nfa.acceptState ? "true" : "false") + "}";
    }
    s += "],\"links\":[";
    bool firstLink = true;
    for (auto& [from, symMap] : nfa.delta) {
        for (auto& [sym, targets] : symMap) {
            string label = (sym == EPS) ? "e" : string(1, sym);
            bool isEps   = (sym == EPS);
            for (int to : targets) {
                if (!firstLink) s += ",";
                s += "{\"source\":" + to_string(from)
                   + ",\"target\":" + to_string(to)
                   + ",\"label\":\"" + label + "\""
                   + ",\"isEps\":"  + (isEps ? "true" : "false") + "}";
                firstLink = false;
            }
        }
    }
    s += "]}";
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// D3.js HTML 생성 — 파스 트리 + ε-NFA 상태전이도
// ─────────────────────────────────────────────────────────────────────────────

void generateHTML(Node* root, const string& regex,
                  int sz, int leaves, int ops, const NFACount& cnt,
                  const ThompsonNFA& nfa) {
    ofstream f("hw2_tree.html");
    if (!f) { cerr << "  [오류] HTML 파일을 열 수 없습니다.\n"; return; }

    string treeJson = toJSON(root);
    string nfaJson  = nfaToJSON(nfa);

    // ── HTML 헤더 ────────────────────────────────────────────────────────────
    f << "<!DOCTYPE html>\n"
         "<html lang=\"ko\">\n"
         "<head>\n"
         "<meta charset=\"UTF-8\">\n"
         "<title>Regex to e-NFA</title>\n"
         "<script src=\"https://d3js.org/d3.v7.min.js\"></script>\n"
         "<style>\n"
         "body{margin:0;background:#1e1e2e;color:#cdd6f4;font-family:monospace}\n"
         "h2{text-align:center;padding:14px 0 4px;font-size:1.15em}\n"
         ".info{text-align:center;font-size:.84em;color:#a6adc8;padding-bottom:8px}\n"
         "hr.sec{border:none;border-top:1px solid #313244;margin:6px 0 0}\n"
         ".operator circle{fill:#89b4fa;stroke:#1e1e2e;stroke-width:2.5px}\n"
         ".leaf     circle{fill:#a6e3a1;stroke:#1e1e2e;stroke-width:2.5px}\n"
         ".node text{font-size:13px;font-weight:bold;fill:#1e1e2e;pointer-events:none}\n"
         ".tlink{fill:none;stroke:#585b70;stroke-width:1.8px}\n"
         ".nfa-edge{fill:none;stroke-width:1.8px}\n"
         ".sym-edge{stroke:#89b4fa}\n"
         ".eps-edge{stroke:#cba6f7;stroke-dasharray:5,3}\n"
         ".nfa-lbl{font-size:11px;font-family:monospace;dominant-baseline:middle}\n"
         ".sym-lbl{fill:#89b4fa}\n"
         ".eps-lbl{fill:#cba6f7}\n"
         ".legend{text-align:center;font-size:.78em;color:#a6adc8;padding:4px 0 8px}\n"
         ".dot{display:inline-block;width:10px;height:10px;border-radius:50%;"
         "margin-right:4px;vertical-align:middle}\n"
         "</style>\n"
         "</head>\n<body>\n";

    // ── 섹션 1: 파스 트리 ────────────────────────────────────────────────────
    f << "<h2>&#9312; &#51221;&#44508;&#54364;&#54788; &#54028;&#49828; &#53944;&#47532; &nbsp;"
         "<code>" << htmlEscape(regex) << "</code></h2>\n";
    f << "<div class=\"info\">"
      << "&#51204;&#52404; &#45432;&#46300; " << sz
      << " &nbsp;|&nbsp; &#45800;&#47568; " << leaves
      << " &nbsp;|&nbsp; &#50672;&#49328;&#51088; " << ops
      << " &nbsp;|&nbsp; NFA &#49345;&#53468; " << cnt.states
      << " &nbsp;|&nbsp; NFA &#51204;&#51060; " << cnt.arcs
      << "</div>\n";
    f << "<svg id=\"tree-svg\"></svg>\n";
    f << "<hr class=\"sec\">\n";

    // ── 섹션 2: ε-NFA ────────────────────────────────────────────────────────
    f << "<h2>&#9313; &epsilon;-NFA &#49345;&#53468;&#51204;&#51060;&#46020;</h2>\n";
    f << "<div class=\"info\" id=\"nfa-info\"></div>\n";
    f << "<div class=\"legend\">"
         "<span class=\"dot\" style=\"background:#89dceb\"></span>&#49884;&#51089; &nbsp;"
         "<span class=\"dot\" style=\"background:#f38ba8\"></span>&#51333;&#44208; &nbsp;"
         "<span class=\"dot\" style=\"background:#45475a;border:1px solid #cdd6f4\"></span>&#51068;&#48152; &nbsp;"
         "<span style=\"color:#89b4fa\">&#9135;&#9135;</span> &#49900;&#48479;&#47673; &nbsp;"
         "<span style=\"color:#cba6f7\">&#9135;&#9135;</span> &epsilon; &#51204;&#51060;"
         "</div>\n";
    f << "<svg id=\"nfa-svg\"></svg>\n";

    // ── JavaScript ──────────────────────────────────────────────────────────
    f << "<script>\n";

    // 파스 트리 시각화
    f << "const treeData = " << treeJson << ";\n";
    f << "const margin={top:50,right:180,bottom:30,left:80};\n"
         "const W=Math.max(900,window.innerWidth)-margin.left-margin.right;\n"
         "const H=Math.max(380,window.innerHeight*0.4)-margin.top-margin.bottom;\n"
         "const tSvg=d3.select('#tree-svg')\n"
         "  .attr('width',W+margin.left+margin.right)\n"
         "  .attr('height',H+margin.top+margin.bottom)\n"
         "  .append('g').attr('transform',`translate(${margin.left},${margin.top})`);\n"
         "const tr=d3.hierarchy(treeData);\n"
         "d3.tree().size([H,W])(tr);\n"
         "tSvg.selectAll('.tlink').data(tr.links()).enter().append('path')\n"
         "  .attr('class','tlink')\n"
         "  .attr('d',d3.linkHorizontal().x(d=>d.y).y(d=>d.x));\n"
         "const tn=tSvg.selectAll('.node').data(tr.descendants())\n"
         "  .enter().append('g')\n"
         "  .attr('class',d=>'node '+(d.children?'operator':'leaf'))\n"
         "  .attr('transform',d=>`translate(${d.y},${d.x})`);\n"
         "tn.append('circle').attr('r',18);\n"
         "tn.append('text').attr('dy','0.35em').attr('text-anchor','middle')\n"
         "  .text(d=>d.data.name);\n";

    // ε-NFA 시각화
    f << "const nfaData=" << nfaJson << ";\n";
    f << "document.getElementById('nfa-info').innerHTML=\n"
      << "  '&#49345;&#53468; |Q| = ' + nfaData.states.length\n"
      << "  + ' &nbsp;|&nbsp; &#51204;&#51060; ' + nfaData.links.length\n"
      << "  + ' &nbsp;|&nbsp; &#49884;&#51089; : q" << nfa.startState << "'\n"
      << "  + ' &nbsp;|&nbsp; &#51333;&#44208; : {q" << nfa.acceptState << "}';\n";

    f << "const nW=Math.max(900,window.innerWidth), nH=560;\n"
         "const nSvg=d3.select('#nfa-svg').attr('width',nW).attr('height',nH);\n"
         "const defs=nSvg.append('defs');\n"
         "function mkArr(id,col){\n"
         "  defs.append('marker').attr('id',id)\n"
         "    .attr('viewBox','0 -5 10 10').attr('refX',8).attr('refY',0)\n"
         "    .attr('markerWidth',6).attr('markerHeight',6).attr('orient','auto')\n"
         "    .append('path').attr('d','M0,-5L10,0L0,5').attr('fill',col);\n"
         "}\n"
         "mkArr('aSym','#89b4fa'); mkArr('aEps','#cba6f7'); mkArr('aStart','#a6e3a1');\n"
         "const ng=nSvg.append('g');\n"
         "const nn=nfaData.states.map(d=>({...d}));\n"
         "const nl=nfaData.links.map(d=>({...d}));\n"
         "const sim=d3.forceSimulation(nn)\n"
         "  .force('link',d3.forceLink(nl).id(d=>d.id).distance(140))\n"
         "  .force('charge',d3.forceManyBody().strength(-700))\n"
         "  .force('center',d3.forceCenter(nW/2,nH/2))\n"
         "  .force('x',d3.forceX(nW/2).strength(0.04))\n"
         "  .force('y',d3.forceY(nH/2).strength(0.04))\n"
         "  .force('col',d3.forceCollide(48));\n"
         "const eLink=ng.selectAll('.nfa-edge').data(nl).enter().append('path')\n"
         "  .attr('class',d=>'nfa-edge '+(d.isEps?'eps-edge':'sym-edge'))\n"
         "  .attr('marker-end',d=>d.isEps?'url(#aEps)':'url(#aSym)');\n"
         "const eLbl=ng.selectAll('.nfa-lbl').data(nl).enter().append('text')\n"
         "  .attr('class',d=>'nfa-lbl '+(d.isEps?'eps-lbl':'sym-lbl'))\n"
         "  .attr('text-anchor','middle').text(d=>d.label);\n"
         "const startArr=ng.append('path')\n"
         "  .attr('fill','none').attr('stroke','#a6e3a1').attr('stroke-width',2)\n"
         "  .attr('marker-end','url(#aStart)');\n"
         "const nNode=ng.selectAll('.nfa-node').data(nn).enter().append('g')\n"
         "  .call(d3.drag()\n"
         "    .on('start',(e,d)=>{if(!e.active)sim.alphaTarget(.3).restart();d.fx=d.x;d.fy=d.y;})\n"
         "    .on('drag', (e,d)=>{d.fx=e.x;d.fy=e.y;})\n"
         "    .on('end',  (e,d)=>{if(!e.active)sim.alphaTarget(0);d.fx=null;d.fy=null;}));\n"
         "nNode.append('circle').attr('r',22)\n"
         "  .attr('fill',d=>d.isStart&&d.isAccept?'#cba6f7':d.isStart?'#89dceb':d.isAccept?'#f38ba8':'#45475a')\n"
         "  .attr('stroke','#cdd6f4').attr('stroke-width',d=>(d.isStart||d.isAccept)?2.5:1.5);\n"
         "nNode.filter(d=>d.isAccept).append('circle')\n"
         "  .attr('r',16).attr('fill','none').attr('stroke','#cdd6f4').attr('stroke-width',1.5);\n"
         "nNode.append('text')\n"
         "  .attr('dy','0.35em').attr('text-anchor','middle')\n"
         "  .attr('font-size','11px').attr('font-weight','bold')\n"
         "  .attr('fill',d=>(d.isStart||d.isAccept)?'#1e1e2e':'#cdd6f4')\n"
         "  .text(d=>'q'+d.id);\n"
         "const R=22;\n"
         "sim.on('tick',()=>{\n"
         "  eLink.attr('d',d=>{\n"
         "    if(d.source.id===d.target.id){\n"
         "      const x=d.source.x,y=d.source.y;\n"
         "      return `M${x-R*.55},${y-R*.83} C${x-80},${y-110} ${x+80},${y-110} ${x+R*.55},${y-R*.83}`;\n"
         "    }\n"
         "    const dx=d.target.x-d.source.x,dy=d.target.y-d.source.y;\n"
         "    const dist=Math.sqrt(dx*dx+dy*dy)||1;\n"
         "    const sx=d.source.x+dx/dist*R,sy=d.source.y+dy/dist*R;\n"
         "    const ex=d.target.x-dx/dist*(R+7),ey=d.target.y-dy/dist*(R+7);\n"
         "    const mx=(sx+ex)/2-dy/dist*30,my=(sy+ey)/2+dx/dist*30;\n"
         "    return `M${sx},${sy} Q${mx},${my} ${ex},${ey}`;\n"
         "  });\n"
         "  eLbl.attr('x',d=>{\n"
         "    if(d.source.id===d.target.id) return d.source.x;\n"
         "    return (d.source.x+d.target.x)/2;\n"
         "  }).attr('y',d=>{\n"
         "    if(d.source.id===d.target.id) return d.source.y-118;\n"
         "    const dx=d.target.x-d.source.x,dy=d.target.y-d.source.y;\n"
         "    const dist=Math.sqrt(dx*dx+dy*dy)||1;\n"
         "    return (d.source.y+d.target.y)/2+dx/dist*30-8;\n"
         "  });\n"
         "  nNode.attr('transform',d=>`translate(${d.x},${d.y})`);\n"
         "  const sn=nn.find(n=>n.isStart);\n"
         "  if(sn) startArr.attr('d',`M${sn.x-65},${sn.y} L${sn.x-R-4},${sn.y}`);\n"
         "});\n";

    f << "</script>\n</body>\n</html>\n";
    f.close();
    cout << "  -> hw2_tree.html 생성됨 (브라우저로 열어 확인하세요)\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    string regex;
    cout << "정규표현 입력: ";
    cin >> regex;

    Node* root = nullptr;
    try {
        RegexParser parser(regex);
        root = parser.parse();
    } catch (const runtime_error& e) {
        cerr << "파싱 오류: " << e.what() << "\n";
        return 1;
    }

    if (!root) {
        cout << "입력이 비어있습니다.\n";
        return 0;
    }

    // ─── 1단계 출력 ───────────────────────────────────────────────────────────
    cout << "\n============================\n";
    cout << " 1단계: 파스 트리\n";
    cout << "============================\n";
    cout << "  연산자: + 합집합  . 접속  * 클리니스타  |  단말: 알파벳.숫자\n\n";
    printTreeRoot(root);

    cout << "\n[리스트 표현]\n  " << serializeList(root) << "\n";

    int sz     = sizeAll(root);
    int leaves = sizeLeaves(root);
    int ops    = sz - leaves;
    NFACount cnt = thompsonCount(root);

    cout << "\n[크기 정보]\n";
    cout << "  전체 노드 수 : " << sz     << "\n";
    cout << "  단말 노드 수 : " << leaves << "\n";
    cout << "  연산자 수    : " << ops    << "\n";
    cout << "\n[Thompson NFA 상태·전이 수 (이론 공식 기반)]\n";
    cout << "  상한  (2n / 4n 공식) : 상태 <= " << 2*sz << ",  전이 <= " << 4*sz << "\n";
    cout << "  이론값 (concat 병합) : 상태  = " << cnt.states << ",  전이  = " << cnt.arcs << "\n";
    cout << "  (실제 구성 결과는 2단계에서 확인)\n";

    // ─── 2단계: ε-NFA 구성 ────────────────────────────────────────────────────
    cout << "\n============================\n";
    cout << " 2단계: epsilon-NFA 구성\n";
    cout << "============================\n";

    ThompsonNFA nfa;
    nfa.construct(root);

    cout << "\n[구성 요소]\n";
    printNFAComponents(nfa);

    cout << "\n[전이 함수 테이블]\n\n";
    printTransitionTable(nfa);

    // ─── HTML 시각화 ──────────────────────────────────────────────────────────
    cout << "\n[D3.js 시각화]\n";
    generateHTML(root, regex, sz, leaves, ops, cnt, nfa);

    deleteTree(root);
    return 0;
}
