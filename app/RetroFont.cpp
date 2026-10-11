#include "RetroFont.h"

#include <utility>

namespace retrofont {
namespace {

// Znaki ASCII 32..126 w zapisie Hershey (plik futural.jhf z dystrybucji Hershey fonts):
// pierwsze dwa znaki to lewa i prawa krawędź litery, dalej pary współrzędnych,
// współrzędna = kod znaku − 'R', " R" = podniesienie pióra.
const char* const kGlyphs[95] = {
    "JZ", //  
    "MWRFRT RRYQZR[SZRY", // !
    "JZNFNM RVFVM", // "
    "H]SBLb RYBRb RLOZO RKUYU", // #
    "H\\PBP_ RTBT_ RYIWGTFPFMGKIKKLMMNOOUQWRXSYUYXWZT[P[MZKX", // $
    "F^[FI[ RNFPHPJOLMMKMIKIIJGLFNFPGSHVHYG[F RWTUUTWTYV[X[ZZ[X[VYTWT", // %
    "E_\\O\\N[MZMYNXPVUTXRZP[L[JZIYHWHUISJRQNRMSKSIRGPFNGMIMKNNPQUXWZY[[[\\Z\\Y", // &
    "MWRHQGRFSGSIRKQL", // '
    "KYVBTDRGPKOPOTPYR]T`Vb", // (
    "KYNBPDRGTKUPUTTYR]P`Nb", // )
    "JZRLRX RMOWU RWOMU", // *
    "E_RIR[ RIR[R", // +
    "NVSWRXQWRVSWSYQ[", // ,
    "E_IR[R", // -
    "NVRVQWRXSWRV", // .
    "G][BIb", // /
    "H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF", // 0
    "H\\NJPISFS[", // 1
    "H\\LKLJMHNGPFTFVGWHXJXLWNUQK[Y[", // 2
    "H\\MFXFRNUNWOXPYSYUXXVZS[P[MZLYKW", // 3
    "H\\UFKTZT RUFU[", // 4
    "H\\WFMFLOMNPMSMVNXPYSYUXXVZS[P[MZLYKW", // 5
    "H\\XIWGTFRFOGMJLOLTMXOZR[S[VZXXYUYTXQVOSNRNOOMQLT", // 6
    "H\\YFO[ RKFYF", // 7
    "H\\PFMGLILKMMONSOVPXRYTYWXYWZT[P[MZLYKWKTLRNPQOUNWMXKXIWGTFPF", // 8
    "H\\XMWPURRSQSNRLPKMKLLINGQFRFUGWIXMXRWWUZR[P[MZLX", // 9
    "NVROQPRQSPRO RRVQWRXSWRV", // :
    "NVROQPRQSPRO RSWRXQWRVSWSYQ[", // ;
    "F^ZIJRZ[", // <
    "E_IO[O RIU[U", // =
    "F^JIZRJ[", // >
    "I[LKLJMHNGPFTFVGWHXJXLWNVORQRT RRYQZR[SZRY", // ?
    "E`WNVLTKQKOLNMMPMSNUPVSVUUVS RQKOMNPNSOUPV RWKVSVUXVZV\\T]Q]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYZX RXKWSWUXV", // @
    "I[RFJ[ RRFZ[ RMTWT", // A
    "G\\KFK[ RKFTFWGXHYJYLXNWOTP RKPTPWQXRYTYWXYWZT[K[", // B
    "H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZV", // C
    "G\\KFK[ RKFRFUGWIXKYNYSXVWXUZR[K[", // D
    "H[LFL[ RLFYF RLPTP RL[Y[", // E
    "HZLFL[ RLFYF RLPTP", // F
    "H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZVZS RUSZS", // G
    "G]KFK[ RYFY[ RKPYP", // H
    "NVRFR[", // I
    "JZVFVVUYTZR[P[NZMYLVLT", // J
    "G\\KFK[ RYFKT RPOY[", // K
    "HYLFL[ RL[X[", // L
    "F^JFJ[ RJFR[ RZFR[ RZFZ[", // M
    "G]KFK[ RKFY[ RYFY[", // N
    "G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF", // O
    "G\\KFK[ RKFTFWGXHYJYMXOWPTQKQ", // P
    "G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF RSWY]", // Q
    "G\\KFK[ RKFTFWGXHYJYLXNWOTPKP RRPY[", // R
    "H\\YIWGTFPFMGKIKKLMMNOOUQWRXSYUYXWZT[P[MZKX", // S
    "JZRFR[ RKFYF", // T
    "G]KFKULXNZQ[S[VZXXYUYF", // U
    "I[JFR[ RZFR[", // V
    "F^HFM[ RRFM[ RRFW[ R\\FW[", // W
    "H\\KFY[ RYFK[", // X
    "I[JFRPR[ RZFRP", // Y
    "H\\YFK[ RKFYF RK[Y[", // Z
    "KYOBOb RPBPb ROBVB RObVb", // [
    "KYKFY^", // backslash
    "KYTBTb RUBUb RNBUB RNbUb", // ]
    "JZRDJR RRDZR", // ^
    "I[Ib[b", // _
    "NVSKQMQORPSORNQO", // `
    "I\\XMX[ RXPVNTMQMONMPLSLUMXOZQ[T[VZXX", // a
    "H[LFL[ RLPNNPMSMUNWPXSXUWXUZS[P[NZLX", // b
    "I[XPVNTMQMONMPLSLUMXOZQ[T[VZXX", // c
    "I\\XFX[ RXPVNTMQMONMPLSLUMXOZQ[T[VZXX", // d
    "I[LSXSXQWOVNTMQMONMPLSLUMXOZQ[T[VZXX", // e
    "MYWFUFSGRJR[ ROMVM", // f
    "I\\XMX]W`VaTbQbOa RXPVNTMQMONMPLSLUMXOZQ[T[VZXX", // g
    "I\\MFM[ RMQPNRMUMWNXQX[", // h
    "NVQFRGSFREQF RRMR[", // i
    "MWRFSGTFSERF RSMS^RaPbNb", // j
    "IZMFM[ RWMMW RQSX[", // k
    "NVRFR[", // l
    "CaGMG[ RGQJNLMOMQNRQR[ RRQUNWMZM\\N]Q][", // m
    "I\\MMM[ RMQPNRMUMWNXQX[", // n
    "I\\QMONMPLSLUMXOZQ[T[VZXXYUYSXPVNTMQM", // o
    "H[LMLb RLPNNPMSMUNWPXSXUWXUZS[P[NZLX", // p
    "I\\XMXb RXPVNTMQMONMPLSLUMXOZQ[T[VZXX", // q
    "KXOMO[ ROSPPRNTMWM", // r
    "J[XPWNTMQMNNMPNRPSUTWUXWXXWZT[Q[NZMX", // s
    "MYRFRWSZU[W[ ROMVM", // t
    "I\\MMMWNZP[S[UZXW RXMX[", // u
    "JZLMR[ RXMR[", // v
    "G]JMN[ RRMN[ RRMV[ RZMV[", // w
    "J[MMX[ RXMM[", // x
    "JZLMR[ RXMR[P_NaLbKb", // y
    "J[XMM[ RMMXM RM[X[", // z
    "KYTBRCQDPFPHQJRKSMSOQQ RRCQEQGRISJTLTNSPORSTTVTXSZR[Q]Q_Ra RQSSUSWRYQZP\\P^Q`RaTb", // {
    "NVRBRb", // |
    "KYPBRCSDTFTHSJRKQMQOSQ RRCSESGRIQJPLPNQPURQTPVPXQZR[S]S_Ra RSSQUQWRYSZT\\T^S`RaPb", // }
    "F^IUISJPLONOPPTSVTXTZS[Q RISJQLPNPPQTTVUXUZT[Q[O", // ~
};

// Polskie litery = litera podstawowa + znak diakrytyczny (te same jednostki, y w dół).
struct Accented
{
    char16_t ch;
    char base;
    const char* accent; // jak w kGlyphs, ale bez krawędzi
};
// Ogonek, kreska (ó, ś…), kropka (ż) i przekreślenie (ł).
const Accented kPolish[] = {
    {u'Ą', 'A', "Z[X]X_Z`[`"}, {u'Ć', 'C', "QDUA"}, {u'Ę', 'E', "Y[W]W_Y`Z`"},
    {u'Ł', 'L', "HSPM"},       {u'Ń', 'N', "QDUA"}, {u'Ó', 'O', "QDUA"},
    {u'Ś', 'S', "QDUA"},       {u'Ź', 'Z', "QDUA"}, {u'Ż', 'Z', "RBQCRDSCRB"},
    {u'ą', 'a', "X[V]V_X`Y`"}, {u'ć', 'c', "QJUG"}, {u'ę', 'e', "T[R]R_T`U`"},
    {u'ł', 'l', "ORUN"},       {u'ń', 'n', "QJUG"}, {u'ó', 'o', "QJUG"},
    {u'ś', 's', "QJUG"},       {u'ź', 'z', "QJUG"}, {u'ż', 'z', "RHQIRJSIRH"},
};

// Rysuje ciąg punktów z zapisu Hershey przesunięty o (dx, 0).
void addStrokes(const char* s, double dx, std::vector<Segment>& out)
{
    bool penDown = false;
    double px = 0, py = 0;
    for (; s[0] && s[1]; s += 2) {
        if (s[0] == ' ' && s[1] == 'R') {
            penDown = false;
            continue;
        }
        const double x = dx + (s[0] - 'R');
        const double y = s[1] - 'R';
        if (penDown)
            out.push_back({px, py, x, y});
        px = x;
        py = y;
        penDown = true;
    }
}

// Glif dla znaku: zapis Hershey i opcjonalny znak diakrytyczny.
std::pair<const char*, const char*> glyph(QChar c)
{
    const char16_t u = c.unicode();
    if (u >= 32 && u <= 126)
        return {kGlyphs[u - 32], nullptr};
    for (const Accented& a : kPolish)
        if (a.ch == u)
            return {kGlyphs[a.base - 32], a.accent};
    return {kGlyphs['?' - 32], nullptr};
}

} // namespace

double layout(const QString& text, std::vector<Segment>& out)
{
    double x = 0;
    for (QChar c : text) {
        const auto [g, accent] = glyph(c);
        const double left = g[0] - 'R';
        const double right = g[1] - 'R';
        addStrokes(g + 2, x - left, out);
        if (accent)
            addStrokes(accent, x - left, out);
        x += right - left;
    }
    return x;
}

double width(const QString& text)
{
    std::vector<Segment> unused;
    return layout(text, unused);
}

} // namespace retrofont
