#ifdef _WIN64
#include "windows.h"
#endif

/*
 * Copied and modified from OCVWarp.cpp - https://github.com/hn-88/OCVWarp
 * 
 * 
 * Take the camera keyframes from multiple osrectxt files and chain them together with minimal interpolation.
 * https://github.com/OpenSpace/OpenSpace/discussions/3294
 * 
 * first commit:
 * Hari Nandakumar
 * 4 June 2024
 * 
 * 
 */

//#define _WIN64
//#define __unix__


#include <stdio.h>
#include <stdlib.h>

#ifdef __unix__
#include <unistd.h>
#endif

#include <string.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
//#include <sys/stat.h>
// this is for mkdir
#include <vector>
#include <iomanip>
#include <cmath>
#include <limits>

#include "tinyfiledialogs.h"
#define CV_PI   3.1415926535897932384626433832795

static const std::string FileHeaderTitle = "OpenSpace_record/playback";
static const std::string HeaderCameraAscii = "camera";
static const std::string HeaderTimeAscii = "time";
static const std::string HeaderScriptAscii = "script";
static const size_t FileHeaderVersionLength = 5;
char FileHeaderVersion[FileHeaderVersionLength+1] = "01.00";
char TargetConvertVersion[FileHeaderVersionLength+1] = "01.00";
static const char DataFormatAsciiTag = 'A';
static const char DataFormatBinaryTag = 'B';
static const size_t keyframeHeaderSize_bytes = 33;
static const size_t saveBufferCameraSize_min = 82;
static const size_t lineBufferForGetlineSize = 2560;

std::ofstream destfileout;
std::string tempstring;
std::stringstream tempstringstream;
std::string word, timeincrstr;
double timeincr;
std::vector<std::string> words;
std::vector<std::string> prevwords;
// https://stackoverflow.com/questions/5607589/right-way-to-split-an-stdstring-into-a-vectorstring
double dvalue[11];
double prevdvalue[11];
// spline lookup table values computed with python code in Python directory - the ybyx.csv file
double splinevals[999] = {0.0,0.10000481925775406,0.10001927703101601,0.10004337331978594,0.10007710812406384,0.1001204814438497,0.10017349327914352,0.10023614362994535,0.10030843249625512,0.10039035987807286,0.10048192577539858,0.1005831301882323,0.10069397311657392,0.10081445456042355,0.10094457451978116,0.1010843329946467,0.10123372998502023,0.10139276549090175,0.1015614395122912,0.10173975204918866,0.10192770310159406,0.10212529266950743,0.10233252075292879,0.10254938735185812,0.10277589246629541,0.10301203609624066,0.10325781824169389,0.10351323890265508,0.10377829807912425,0.10405299577110141,0.10433733197858652,0.10463130670157961,0.10493491994008064,0.10524817169408965,0.10557106196360666,0.10590359074863163,0.10624575804916456,0.10659756386520546,0.1069590081967543,0.10733009104381117,0.10771081240637596,0.10810117228444878,0.1085011706780295,0.10891080758711823,0.10933008301171492,0.10975899695181957,0.11019754940743218,0.11064574037855278,0.11110356986518131,0.11157103786731787,0.11204814438496238,0.11253488941811489,0.11303127296677534,0.11353729503094376,0.11405295561062012,0.11457825470580449,0.11511319231649682,0.1156577684426971,0.11621198308440538,0.1167758362416216,0.11734932791434582,0.11793245810257798,0.11852522680631815,0.11912763402556625,0.11973967976032235,0.1203613640105864,0.12099268677635842,0.12163364805763845,0.12228424785442638,0.12294448616672231,0.12361436299452622,0.1242938783378381,0.12498303219665795,0.12568182457098578,0.12639025546082153,0.12710832486616527,0.127836032787017,0.1285733792233767,0.1293203641752443,0.13007698764262002,0.1308432496255036,0.13161915012389516,0.1324046891377947,0.13319986666720224,0.13400468271211774,0.13481913727254116,0.1356432303484726,0.13647696193991202,0.13732033204685934,0.1381733406693147,0.13903598780727797,0.13990827346074924,0.1407901976297285,0.14168176031421567,0.1425829615142109,0.14349380122971403,0.1444142794607251,0.14534439620724424,0.14628415146927126,0.14723354524680635,0.1481925775398493,0.14916124834840033,0.15013955767245926,0.15112750551202614,0.15212509186710105,0.15313231673768388,0.15414918012377468,0.1551756820253735,0.15621182244248022,0.157257601375095,0.15831301882321766,0.15937807478684832,0.16045276926598698,0.16153710226063359,0.16263107377078817,0.16373468379645073,0.1648479323376212,0.16597081939429972,0.16710334496648618,0.1682455090541806,0.16939731165738298,0.17055875277609334,0.17172983241031167,0.172910550560038,0.1741009072252723,0.17530090240601454,0.17651053610226472,0.17772980831402288,0.1789587190412891,0.18019726828406318,0.18144545604234533,0.1827032823161354,0.18397074710543337,0.18524785041023936,0.1865345922305534,0.1878309725663753,0.18913699141770524,0.1904526487845431,0.19177794466688905,0.19311287906474278,0.19445745197810457,0.19581166340697437,0.19717551335135208,0.19854900181123777,0.19993212878663144,0.2013248942775331,0.2027272982839427,0.20413934080586033,0.20556102184328587,0.20699234139621941,0.20843329946466088,0.20988389604861038,0.2113441311480678,0.21281400476303317,0.21429351689350654,0.21578266753948783,0.21728145670097715,0.2187898843779745,0.22030795057047972,0.22183565527849294,0.22337299850201411,0.22491998024104334,0.22647660049558044,0.2280428592656255,0.2296187565511786,0.2312042923522397,0.23279946666880866,0.23440427950088566,0.23601873084847055,0.23764282071156348,0.23927654909016438,0.2409199159842733,0.2425729213938901,0.24423556531901489,0.24590784775964766,0.24758976871578836,0.2492813281874371,0.2509825261745938,0.2526933626772584,0.2544138376954311,0.25614395122911165,0.2578837032783001,0.25963309384299676,0.26139212292320113,0.2631607905189136,0.26493909663013415,0.2667270412568625,0.26852462439909885,0.27033184605684324,0.2721487062300955,0.27397520491885585,0.27581134212312414,0.27765711784290026,0.27951253207818455,0.2813775848289767,0.2832522760952768,0.2851366058770849,0.28703057417440103,0.288934180987225,0.290847426315557,0.2927703101593971,0.29470283251874496,0.29664499339360095,0.2985967927839649,0.3005582306898367,0.30252930711121656,0.30451002204810423,0.30650037550050013,0.30850036746840376,0.3105099979518155,0.31252926695073524,0.3145581744651628,0.31659672049509846,0.3186449050405421,0.32070272810149375,0.32277018967795323,0.3248472897699208,0.32693402837739616,0.32903040550037965,0.33113642113887104,0.33325207529287043,0.33537736796237777,0.3375122991473931,0.3396568688479163,0.3418110770639476,0.34397492379548683,0.346148409042534,0.3483315328050893,0.3505242950831524,0.35272669587672345,0.3549387351858026,0.3571604130103897,0.35939172935048463,0.36163268420608763,0.36388327757719857,0.36614350946381746,0.36841337986594436,0.3706928887835792,0.3729820362167222,0.37528082216537284,0.37758924662953175,0.3799073096091984,0.3822350111043732,0.3845723511150558,0.3869193296412465,0.38927594668294513,0.3916422022401517,0.3940180963128663,0.39640362890108877,0.3987988000048194,0.40120360480480005,0.40361792815725095,0.4060415467828491,0.40847423617026607,0.41091577534378554,0.413365946793977,0.41582453640999434,0.4182913334134565,0.42076613029386356,0.42324872274551195,0.42573890960586197,0.428236492795325,0.43074127725842853,0.43325307090632276,0.4357716845605952,0.43829693189835645,0.4408286293985654,0.4433665962895597,0.44591065449776124,0.44846062859752633,0.4510163457621098,0.4535776357157149,0.4561443306866002,0.4587162653612186,0.4612932768393597,0.4638752045902701,0.4664618904097296,0.46905317837805505,0.4716489148190126,0.4742489482596111,0.47685312939075986,0.47946131102876416,0.48207334807764174,0.4846890974922364,0.4873084182421125,0.48993117127620933,0.49255721948823594,0.4951864276827905,0.49781866254218526,0.50045379259396,0.5030916881790672,0.5057322214207148,0.5083752661938477,0.5110206980952543,0.5136683944142837,0.5163182341041581,0.5189700977538678,0.5216238675606308,0.524279427302912,0.5269366623139781,0.5295954594559863,0.5322557070945867,0.5349172950740299,0.5375801146927679,0.5402440586795362,0.5429090211699065,0.5455748976832994,0.5482415851004457,0.5509089816412879,0.5535769868433094,0.556245501540284,0.5589144278414357,0.5615836691109982,0.5642531299481675,0.5669227161674357,0.5695923347793019,0.5722618939713474,0.57493130308967,0.5776004726206677,0.5802693141731663,0.5829377404608816,0.5856056652852087,0.5882730035183324,0.5909396710866529,0.5936055849545153,0.5962706631082422,0.5989348245404588,0.6015979892347066,0.6042600781503376,0.6069210132076837,0.6095807172734978,0.6122391141466557,0.6148961285441201,0.6175516860871538,0.6202057132877828,0.6228581375355023,0.6255088870842171,0.6281578910394183,0.6308050793455859,0.6334503827738119,0.6360937329096482,0.6387350621411607,0.6413743036471988,0.6440113913858672,0.6466462600831998,0.649278845222029,0.6519090830310522,0.6545369104740816,0.657162265239483,0.659785085729797,0.6624053110515336,0.6650228810051462,0.6676377360751747,0.6702498174205542,0.6728590668650914,0.6754654268880986,0.6780688406151881,0.6806692518092213,0.6832666048614061,0.6858608447825477,0.6884519171944424,0.6910397683214144,0.6936243449819955,0.696205594580738,0.6987834651001664,0.7013579050928607,0.7039288636736661,0.706496290512036,0.7090601358244939,0.7116203503672218,0.714176885428768,0.7167296928228707,0.7192787248814,0.7218239344474139,0.7243652748683204,0.7269026999891567,0.72943616414597,0.7319656221593049,0.7344910293277969,0.7370123414218626,0.7395295146774934,0.7420425057901453,0.7445512719087255,0.747055770629671,0.7495559599911245,0.7520517984671944,0.7545432449623098,0.7570302588056602,0.759512799745722,0.7619908279448687,0.7644643039740651,0.7669331888076415,0.7693974438181503,0.7718570307712981,0.7743119118209563,0.7767620495042493,0.7792074067367138,0.7816479468075337,0.7840836333748455,0.7865144304611151,0.7889403024485847,0.7913612140747847,0.793777130428116,0.796188016943498,0.7985938393980765,0.8009945639070022,0.8033901569192662,0.8057805852135996,0.8081658158944318,0.8105458163879097,0.8129205544379732,0.8152899981024906,0.8176541157494475,0.8200128760531928,0.8223662479907393,0.8247142008381154,0.8270567041667733,0.8293937278400458,0.8317252420096541,0.8340512171122682,0.836371623866112,0.8386864332676225,0.8409956165881517,0.843299145370717,0.8455969914267982,0.8478891268331783,0.8501755239288309,0.8524561553118486,0.8547309938364172,0.8570000126098294,0.8592631849895439,0.8615204845802807,0.8637718852311613,0.8660173610328861,0.8682568863149495,0.8704904356428979,0.8727179838156206,0.8749395058626812,0.8771549770416832,0.8793643728356737,0.8815676689505809,0.8837648413126882,0.8859558660661396,0.8881407195704828,0.8903193783982416,0.8924918193325246,0.8946580193646637,0.8968179556918845,0.8989716057150086,0.9011189470361873,0.9032599574566635,0.9053946149745646,0.9075228977827255,0.9096447842665384,0.9117602530018323,0.9138692827527811,0.9159718524698379,0.9180679412876965,0.9201575285232803,0.9222405936737583,0.9243171164145841,0.9263870765975633,0.9284504542489451,0.9305072295675375,0.9325573829228494,0.934600894853253,0.936637746064174,0.938667917426301,0.9406913899738225,0.9427081449026816,0.9447181635688553,0.9467214274866571,0.9487179183270587,0.9507076179160339,0.952690508232924,0.9546665714088234,0.9566357897249846,0.9585981456112463,0.9605536216444761,0.9625022005470394,0.9644438651852801,0.9663785985680275,0.9683063838451148,0.970227204305922,0.9721410433779334,0.9740478846253131,0.9759477117475012,0.977840508577821,0.9797262590821118,0.9816049473573701,0.9834765576304134,0.9853410742565581,0.9871984817183123,0.989048764624087,0.9908919077069216,0.9927278958232248,0.9945567139515311,0.9963783471912717,0.9981927807615618,1.0,1.0017999903614845,1.0035927374170421,1.0053782268526703,1.007156444468198,1.0089273761761521,1.0106910080006437,1.0124473260762656,1.0141963166470018,1.0159379660651524,1.0176722607902675,1.0193991873880968,1.0211187325295519,1.0228308829896768,1.0245356256466336,1.0262329474807008,1.027922835573281,1.0296052771059196,1.03128025935934,1.0329477697124814,1.034607795641557,1.036260324719116,1.03790534461312,1.039542843086029,1.0411728079938987,1.0427952272854843,1.044410089001364,1.0460173812730575,1.0476170923221715,1.0492092104595419,1.0507937240843903,1.0523706216834923,1.053939891830353,1.055501523184389,1.0570555044901266,1.0586018245764006,1.0601404723555703,1.061671436822738,1.0631947070549785,1.0647102722105786,1.0662181215282838,1.067718244326551,1.069210630002815,1.0706952680327564,1.0721721479695847,1.0736412594433238,1.0751025921601067,1.07655613590148,1.0780018805237135,1.0794398159571192,1.0808699322053765,1.082292219344866,1.0837066675240068,1.0851132669626087,1.0865120079512236,1.0879028808505067,1.0892858760905881,1.0906609841704429,1.0920281956572793,1.0933875011859222,1.0947388914582095,1.0960823572423977,1.097417889372564,1.0987454787480238,1.1000651163327522,1.1013767931548084,1.1026805003057705,1.103976228940174,1.1052639702749545,1.1065437155889017,1.1078154562221147,1.1090791835754623,1.110334889110054,1.11158256434671,1.1128222008654438,1.1140537903049441,1.1152773243620637,1.1164927947913197,1.1177001934043866,1.1188995120696077,1.1200907427115023,1.1212738773102837,1.1224489079013784,1.1236158265749527,1.124774625475441,1.1259252968010856,1.1270678328034727,1.128202225787078,1.1293284681088185,1.1304465521776037,1.1315564704538965,1.1326582154492748,1.1337517797259982,1.1348371558965844,1.1359143366233797,1.1369833146181436,1.1380440826416314,1.1390966335031838,1.1401409600603207,1.1411770552183385,1.1422049119299083,1.1432245231946863,1.1442358820589174,1.1452389816150523,1.1462338150013627,1.147220375401562,1.1481986560444297,1.1491686502034408,1.1501303511963958,1.1510837523850572,1.1520288471747884,1.1529656290141967,1.1538940913947784,1.1548142278505693,1.155726031957798,1.1566294973345412,1.1575246176403837,1.1584113865760832,1.1592897978832333,1.1601598453439361,1.161021522780475,1.1618748240549883,1.1627197430691498,1.1635562737638514,1.1643844101188867,1.1652041461526415,1.1660154759217838,1.1668183935209573,1.1676128930824803,1.1683989687760445,1.1691766148084184,1.1699458254231532,1.1707065949002897,1.1714589175560732,1.1722027877426637,1.172938199847854,1.1736651482947906,1.1743836275416941,1.175093632081585,1.1757951564420077,1.176488195184766,1.1771727429056509,1.1778487942341782,1.1785163438333226,1.1791753863992618,1.1798259166611147,1.1804679293806895,1.1811014193522271,1.1817263814021541,1.1823428103888327,1.182950701202313,1.1835500487640924,1.1841408480268723,1.184723093974319,1.1852967816208273,1.1858619060112838,1.1864184622208362,1.1869664453546618,1.1875058505477387,1.1880366729646197,1.1885589077992078,1.1890725502745336,1.1895775956425358,1.1900740391838416,1.190561876207552,1.1910411020510252,1.191511712079666,1.191973701686715,1.1924270662930392,1.1928718013469253,1.1933079023238748,1.1937353647264024,1.1941541840838314,1.194564355952099,1.1949658759135533,1.1953587395767615,1.195742942576313,1.196118480572629,1.19648534925177,1.196843544325248,1.197193061529838,1.197533896627394,1.1978660454046621,1.1981895036731003,1.198504267268697,1.1988103320517918,1.1991076939068963,1.1993963487425197,1.1996762924909952,1.1999475211083026,1.200210030573901,1.200463816890556,1.2007088760841724,1.2009452042036282,1.2011727973206041,1.2013916515294254,1.2016017629468945,1.2018031277121308,1.2019957419864127,1.2021796019530147,1.202354703817054,1.202521043805332,1.202678618166181,1.2028274231693097,1.202967455105652,1.2030987102872173,1.2032211850469372,1.2033348757385223,1.2034397787363116,1.2035358904351283,1.2036232072501352,1.2037017256166913,1.2037714419902101,1.203832352846018,1.2038844546792153,1.2039277440045382,1.20396221735622,1.2039878712878564,1.2040047023722698,1.204012707201375,1.2040118823860484,1.2040022245559927,1.2039837303596101,1.203956396463871,1.2039202195541854,1.203875196334276,1.2038213235260518,1.2037585978694836,1.203687016122478,1.2036065750607563,1.2035172714777307,1.203419102184384,1.203312064009149,1.2031961537977893,1.203071368413282,1.2029377047356993,1.2027951596620916,1.202643730106375,1.2024834129992135,1.202314205287908,1.2021361039362823,1.2019491059245722,1.2017532082493145,1.2015484079232364,1.2013347019751495,1.201112087449837,1.2008805614079503,1.2006401209259014,1.2003907630957567,1.200132485025134,1.1998652854456613,1.199589200043375,1.1993043012126872,1.1990106625650787,1.1987083573210005,1.1983974583124657,1.1980780379856233,1.1977501684033092,1.1974139212475767,1.1970693678222115,1.1967165790552203,1.1963556255013055,1.1959865773443166,1.1956095043996837,1.195224476116832,1.1948315615815777,1.1944308295185027,1.1940223482933148,1.1936061859151863,1.193182410039074,1.192751087968024,1.1923122866554563,1.1918660727074324,1.1914125123849026,1.190951671605942,1.1904836159479644,1.1900084106499176,1.1895261206144678,1.1890368104101607,1.1885405442735706,1.1880373861114304,1.187527399502746,1.1870106477008953,1.186487193635708,1.1859570999155344,1.185420428829294,1.1848772423485103,1.1843276021293307,1.1837715695145286,1.183209205535493,1.1826405709142018,1.1820657260651797,1.1814847310974415,1.1808976458164209,1.180304529725886,1.179705442029836,1.17910044163439,1.1784895871496543,1.1778729368915843,1.177250548883822,1.17662248085953,1.1759887902632042,1.1753495342524767,1.174704769699904,1.174054553194742,1.1733989410447085,1.1727379892777308,1.1720717536436822,1.1714002896161042,1.170723652393917,1.1700418969031168,1.1693550777984596,1.168663249465134,1.1679664660204223,1.1672647813153465,1.1665582489363033,1.1658469222066912,1.1651308541885161,1.1644100976839966,1.1636847052371493,1.1629547291353652,1.1622202214109751,1.161481233842802,1.1607378179577046,1.1599900250321067,1.1592379060935174,1.1584815119220406,1.1577208930518692,1.1569560997727775,1.1561871821315908,1.155414189933657,1.1546371727442957,1.153856179890246,1.1530712604610989,1.152282463310721,1.151489837058668,1.150693430091587,1.1498932905646109,1.1490894664027385,1.1482820053022122,1.1474709547318773,1.1466563619345358,1.1458382739282944,1.1450167375078926,1.144191799246033,1.1433635054946933,1.1425319023864338,1.141697035835694,1.1408589515400798,1.1400176949816427,1.1391733114281488,1.1383258459343388,1.1374753433431808,1.136621848287112,1.1357654051892738,1.1349060582647363,1.1340438515217155,1.1331788287627809,1.1323110335860567,1.1314405093864128,1.1305672993566451,1.1296914464886558,1.128812993574615,1.1279319832081214,1.127048457785353,1.1261624595062094,1.1252740303754454,1.1243832122037984,1.1234900466091078,1.1225945750174258,1.1216968386641202,1.1207968785949711,1.1198947356672584,1.1189904505508448,1.1180840637292462,1.1171756155006993,1.1162651459792206,1.115352695095658,1.1144383025987334,1.1135220080560828,1.1126038508552825,1.1116838702048761,1.1107621051353882,1.1098385945003335,1.1089133769772208,1.1079864910685473,1.107057975102787,1.106127867235374,1.105196205449676,1.1042630275579666,1.1033283712023836,1.1023922738558871,1.1014547728232087,1.1005159052417939,1.0995757080827377,1.0986342181517197,1.0976914720899216,1.0967475063749492,1.095802357321744,1.0948560610834874,1.0939086536525022,1.0929601708611447,1.0920106483826928,1.091060121732228,1.0901086262675121,1.0891561971898562,1.0882028695449844,1.0872486782238955,1.086293657963712,1.0853378433485308,1.084381268810263,1.0834239686294707,1.0824659769361968,1.0815073277107923,1.0805480547847344,1.0795881918414403,1.0786277724170783,1.0776668299013707,1.076705397538391,1.075743508427359,1.0747811955234283,1.0738184916384692,1.0728554294418464,1.0718920414611917,1.070928360083173,1.0699644175542544,1.0690002459814576,1.068035877333113,1.0670713434396055,1.066106675994123,1.06514190655339,1.0641770665384036,1.0632121872351623,1.0622472997953891,1.061282435237253,1.060317624446082,1.0593528981750746,1.058388287046005,1.057423821549926,1.056459532047863,1.0554954487715085,1.0545316018239097,1.053568021180152,1.0526047366880378,1.051641778068761,1.0506791749175792,1.0497169567044788,1.048755152774837,1.0477937923500804,1.0468329045283382,1.0458725182850925,1.0449126624738225,1.0439533658266478,1.0429946569549642,1.0420365643500777,1.041079116383834,1.040122341309244,1.039166267261105,1.0382109222566183,1.0372563341960033,1.0363025308631073,1.0353495399260118,1.0343973889376339,1.0334461053363264,1.032495716446471,1.0315462494790706,1.0305977315323365,1.0296501895922716,1.0287036505332519,1.0277581411186016,1.0268136880011667,1.025870317723885,1.024928056720351,1.02398693131538,1.023046967725565,1.022108192059834,1.021170630320001,1.0202343084013155,1.019299252093008,1.0183654870788301,1.0174330389375963,1.0165019331437168,1.0155721950677312,1.0146438499768364,1.0137169230354128,1.0127914393055464,1.0118674237475482,1.0109449012204712,1.0100238964826214,1.009104434192069,1.0081865389071554,1.007270235086996,1.0063555470919812,1.0054424991842743,1.0045311155283054,1.0036214201912643,1.002713437143587,1.0018071902594443,1.000902703317221,1.0};
bool useSpline;

enum class DataMode {
        Ascii = 0,
        Binary,
        Unknown
    };

enum class SessionState {
        Idle = 0,
        Recording,
        Playback,
        PlaybackPaused
    };

struct Timestamps {
        double timeOs;
        double timeRec;
        double timeSim;
    };
DataMode _recordingDataMode = DataMode::Binary;


std::string readHeaderElement(std::ifstream& stream,
                                                size_t readLenChars)
{
    std::vector<char> readTemp(readLenChars);
    stream.read(readTemp.data(), readLenChars);
    return std::string(readTemp.begin(), readTemp.end());
}

std::string readHeaderElement(std::stringstream& stream,
                                                size_t readLenChars)
{
    std::vector<char> readTemp = std::vector<char>(readLenChars);
    stream.read(readTemp.data(), readLenChars);
    return std::string(readTemp.begin(), readTemp.end());
}

bool checkIfValidRecFile(std::string _playbackFilename) {
	//////////////////////////////////////////////////
	// from https://github.com/OpenSpace/OpenSpace/blob/0ff646a94c8505b2b285fdd51800cdebe0dda111/src/interaction/sessionrecording.cpp#L363
	// checking the osrectxt format etc
	/////////////////////////
	// std::string _playbackFilename = OpenFileName;
	std::ifstream _playbackFile;
	std::string _playbackLineParsing;
	std::ofstream _recordFile;
	int _playbackLineNum = 1;

	_playbackFile.open(_playbackFilename, std::ifstream::in);
	// Read header
	const std::string readBackHeaderString = readHeaderElement(
        _playbackFile,
        FileHeaderTitle.length()
	    );
	    if (readBackHeaderString != FileHeaderTitle) {
	        std::cerr << "Specified osrec file does not contain expected header" << std::endl;
	        return false;
	    }
	    readHeaderElement(_playbackFile, FileHeaderVersionLength);
	    std::string readDataMode = readHeaderElement(_playbackFile, 1);
	    if (readDataMode[0] == DataFormatAsciiTag) {
	        _recordingDataMode = DataMode::Ascii;
	    }
	    else if (readDataMode[0] == DataFormatBinaryTag) {
	        _recordingDataMode = DataMode::Binary;
		std::cerr << "Sorry, currently only Ascii data type is supported.";
	        return false;
		    
	    }
	    else {
	        std::cerr << "Unknown data type in header (should be Ascii or Binary)";
	        return false;
	    }
		// throwaway newline character(s)
	    std::string lineEnd = readHeaderElement(_playbackFile, 1);
	    bool hasDosLineEnding = (lineEnd == "\r");
	    if (hasDosLineEnding) {
	        // throwaway the second newline character (\n) also
	        readHeaderElement(_playbackFile, 1);
	    }
	
	    if (_recordingDataMode == DataMode::Binary) {
	        // Close & re-open the file, starting from the beginning, and do dummy read
	        // past the header, version, and data type
	        _playbackFile.close();
	        _playbackFile.open(_playbackFilename, std::ifstream::in | std::ios::binary);
	        const size_t headerSize = FileHeaderTitle.length() + FileHeaderVersionLength
	            + sizeof(DataFormatBinaryTag) + sizeof('\n');
	        std::vector<char> hBuffer;
	        hBuffer.resize(headerSize);
	        _playbackFile.read(hBuffer.data(), headerSize);
	    }
	
	    if (!_playbackFile.is_open() || !_playbackFile.good()) {
	        std::cerr << "Unable to open file";        
	        return false;
	    }
	_playbackFile.close();
	return true;
	
}

std::string getLastCameraKfstring(std::string _playbackFilename) {
	std::ifstream _playbackFile;
	_playbackFile.open(_playbackFilename, std::ifstream::in);
	char line[lineBufferForGetlineSize];
	while(_playbackFile.getline(line, lineBufferForGetlineSize)) {
		if(line[0] == 'c') {
			if(line[1] == 'a') {
				if(line[2] == 'm') {
					tempstring = line;
				}
			}
		}
		
	}
	return tempstring;	
}

struct CameraPos {
	double xpos;
	double ypos;
	double zpos;
	double xrot;
	double yrot;
	double zrot;
	double wrot;
	double scale;	
};

struct XYZ {
	double x;
	double y;
	double z;
};

struct RThetaPhi {
	double r;
	double theta;
	double phi;
};

RThetaPhi toSpherical (XYZ p) {
	// https://gamedev.stackexchange.com/questions/66906/spherical-coordinate-from-cartesian-coordinate
	// https://neutrium.net/mathematics/converting-between-spherical-and-cartesian-co-ordinate-systems/ has the wrong formula
	// https://mathworld.wolfram.com/SphericalCoordinates.html
	RThetaPhi a;
	a.r 		= std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
	a.phi 		= std::acos(p.z / a.r);
	a.theta 	= std::atan2(p.y, p.x);	
	return a;
}

XYZ toCartesian (RThetaPhi a) {
	// https://neutrium.net/mathematics/converting-between-spherical-and-cartesian-co-ordinate-systems/
	XYZ p;
	p.x = a.r * std::sin(a.phi) * std::cos(a.theta);
	p.y = a.r * std::sin(a.phi) * std::sin(a.theta);
	p.z = a.r * std::cos(a.phi);
	return p;
}

class CameraKeyFrame {
  public:
	Timestamps ts;
	CameraPos pos;
	std::string fstring;
  
	void incrementOnlyTwoTimestamps(double incrementval) {
		ts.timeOs  += incrementval;
		ts.timeRec += incrementval;
	};
	void incrementAllTimestamps(double incremValtimeRec, double incremValtimeSim) {
		ts.timeOs  += incremValtimeRec;
		ts.timeRec += incremValtimeRec;
		ts.timeSim += incremValtimeSim;
	};
	void setTimestampsFrom(CameraKeyFrame kf) {
		ts.timeOs  = kf.ts.timeOs;
		ts.timeRec = kf.ts.timeRec;
		//ts.timeSim = kf.ts.timeSim; this should not be set, since we would need it if !ignoreTime
		
	};
	std::string getCamkfAscii() {
		// https://github.com/OpenSpace/OpenSpace/blob/807699a8902fcb9368a495650b99d537fea8dd0e/src/interaction/sessionrecording.cpp#L820
		std::stringstream ss;
		ss << HeaderCameraAscii << " " << ts.timeOs << " " << ts.timeRec << " " 
			<< std::fixed << std::setprecision(3) << ts.timeSim << " " ;
		// << "position" << std::endl;
		// https://github.com/OpenSpace/OpenSpace/blob/807699a8902fcb9368a495650b99d537fea8dd0e/include/openspace/network/messagestructures.h#L190
		ss << std::setprecision(std::numeric_limits<double>::max_digits10);
		ss << pos.xpos << ' ' << pos.ypos << ' ' << pos.zpos << ' ';
	        // Add camera rotation
	        ss << pos.xrot << ' '
	            << pos.yrot << ' '
	            << pos.zrot << ' '
	            << pos.wrot << ' ';
	        ss << std::scientific << pos.scale;
		ss << ' ' << fstring << std::endl;			
		return ss.str();
	};
	void populateCamkfAscii(std::string line) {
		std::stringstream ss(line);
		std::string w;
		std::vector<std::string> vwords;
		while(getline(ss, w, ' ')) {
			vwords.push_back(w);
		}
		ts.timeOs  = atof(vwords[1].c_str());
		ts.timeRec = atof(vwords[2].c_str());
		ts.timeSim = atof(vwords[3].c_str());

		pos.xpos	= atof(vwords[4].c_str());
		pos.ypos	= atof(vwords[5].c_str());
		pos.zpos	= atof(vwords[6].c_str());

		pos.xrot	= atof(vwords[7].c_str());
		pos.yrot	= atof(vwords[8].c_str());
		pos.zrot	= atof(vwords[9].c_str());
		pos.wrot	= atof(vwords[10].c_str());
		pos.scale	= atof(vwords[11].c_str());
		
		fstring = vwords[12];
		for (int i = 13; i < vwords.size(); i++) {
			fstring += " " + vwords[i];
		}		
	};	// end populateCamkfAscii
	
	void copyFrom(CameraKeyFrame kf) {
		ts.timeOs  = kf.ts.timeOs;
		ts.timeRec = kf.ts.timeRec;
		ts.timeSim = kf.ts.timeSim;
		pos.xpos = kf.pos.xpos;
		pos.ypos = kf.pos.ypos;
		pos.zpos = kf.pos.zpos;
		pos.xrot = kf.pos.xrot;
		pos.yrot = kf.pos.yrot;
		pos.zrot = kf.pos.zrot;
		pos.wrot = kf.pos.wrot;
		pos.scale = kf.pos.scale;
			
		fstring = kf.fstring.c_str();
	};
	CameraPos getCameraPos() {
		return pos;
	};
	void setCameraPos(CameraPos p) {
		// set the postition string from the data in struct pos 
		pos = p;
	};
	void normalizeQuat() {
		double quatmodulus;
		quatmodulus = std::sqrt( pos.xrot*pos.xrot +  pos.yrot* pos.yrot +  pos.zrot* pos.zrot +  pos.wrot* pos.wrot);
		pos.xrot /= quatmodulus;
		pos.yrot /= quatmodulus;
		pos.zrot /= quatmodulus;
		pos.wrot /= quatmodulus;
	};
    
}; // end class CameraKeyFrame

class ScriptKeyFrame {
  public:
	Timestamps ts;
	std::string scr;
  
	void incrementOnlyTwoTimestamps(double incrementval) {
		ts.timeOs  += incrementval;
		ts.timeRec += incrementval;
	};
	void incrementAllTimestamps(double incremValtimeRec, double incremValtimeSim) {
		ts.timeOs  += incremValtimeRec;
		ts.timeRec += incremValtimeRec;
		ts.timeSim += incremValtimeSim;
	};
	void setTimestampsFrom(CameraKeyFrame kf) {
		ts.timeOs  = kf.ts.timeOs;
		ts.timeRec = kf.ts.timeRec;
		ts.timeSim = kf.ts.timeSim;
	};
	
	std::string getScrkfAscii() {
		// https://github.com/OpenSpace/OpenSpace/blob/95b4decccad31f7f703bcb8141bd854ba78c7938/src/interaction/sessionrecording.cpp#L839
		std::stringstream ss;
		ss << HeaderScriptAscii << " " << ts.timeOs << " " << ts.timeRec << " " 
			<< std::fixed << std::setprecision(3) << ts.timeSim << " " << scr << std::endl;
		// which should look like
		// script 188.098 0 768100269.188  1 openspace.time.setPause(true)
		// script 188.098 0 768100269.188  1 openspace.time.setDeltaTime(-3600)
		return ss.str();
	};
	
	void setScriptString(std::string s) {
		scr = s.c_str();
	};
    
}; // end class ScriptKeyFrame

void interpolatebetween(CameraKeyFrame kf1, CameraKeyFrame kf2) {
	// optional - add a check to make sure kf1 and kf2 have the same object in Focus
	// add 999 points between kf1 and kf2 equi-spaced in time and spherical co-ords 

	// first find the increments for each of the coords, 
	// after normalizing them
	kf1.normalizeQuat();
	kf2.normalizeQuat();
	
	double xrotincr = (kf2.pos.xrot - kf1.pos.xrot) / 1000;
	double yrotincr = (kf2.pos.yrot - kf1.pos.yrot) / 1000;
	double zrotincr = (kf2.pos.zrot - kf1.pos.zrot) / 1000;
	double wrotincr = (kf2.pos.wrot - kf1.pos.wrot) / 1000;
	double scaleincr = (kf2.pos.scale - kf1.pos.scale) / 1000;

	double timeincr = (kf2.ts.timeOs - kf1.ts.timeOs) / 1000;
	double timeSimincr = (kf2.ts.timeSim - kf1.ts.timeSim) / 1000;

	XYZ p1, p2;
	RThetaPhi a1, a2;
	p1.x = kf1.pos.xpos;
	p1.y = kf1.pos.ypos;
	p1.z = kf1.pos.zpos;
	p2.x = kf2.pos.xpos;
	p2.y = kf2.pos.ypos;
	p2.z = kf2.pos.zpos;
	a1 = toSpherical(p1);
	a2 = toSpherical(p2);
	double rincr = (a2.r - a1.r) / 1000;
	double thetaincr = (a2.theta - a1.theta) / 1000;
	double phiincr = (a2.phi - a1.phi) / 1000;

	// now loop through the intermediate points
	CameraKeyFrame ikf;
	XYZ p;
	RThetaPhi a;
	ikf.copyFrom(kf1);
		
	for (int i = 1; i < 1000; i++) {
		// add suitable increment to interpolated CameraKeyFrame
		ikf.incrementAllTimestamps(timeincr, timeSimincr);
		
		p.x = ikf.pos.xpos;
		p.y = ikf.pos.ypos;
		p.z = ikf.pos.zpos;
		a = toSpherical(p);
		a.r += rincr;
		a.theta += thetaincr;
		a.phi += phiincr;
		p = toCartesian(a);
		ikf.pos.xpos = p.x;
		ikf.pos.ypos = p.y;
		ikf.pos.zpos = p.z;
		// ikf.pos.xrot += xrotincr; // this causes discontinuity at end due to normalization
		ikf.pos.xrot = i*xrotincr + kf1.pos.xrot;
		ikf.pos.yrot = i*yrotincr + kf1.pos.yrot;
		ikf.pos.zrot = i*zrotincr + kf1.pos.zrot;
		ikf.pos.wrot = i*wrotincr + kf1.pos.wrot;
		ikf.pos.scale += scaleincr;
		// need to normalize the rot quat
		ikf.normalizeQuat();
		
		destfileout << ikf.getCamkfAscii();
	}
	
}

void splineinterpolatebetween(CameraKeyFrame kf1, CameraKeyFrame kf2) {
	// optional - add a check to make sure kf1 and kf2 have the same object in Focus
	// add 999 points between kf1 and kf2 equi-spaced in time, but multiplied by a spline lookup table in position using spherical co-ords 

	// first find the increments for each of the coords
	
	double xrotincr = (kf2.pos.xrot - kf1.pos.xrot) / 1000;
	double yrotincr = (kf2.pos.yrot - kf1.pos.yrot) / 1000;
	double zrotincr = (kf2.pos.zrot - kf1.pos.zrot) / 1000;
	double wrotincr = (kf2.pos.wrot - kf1.pos.wrot) / 1000;
	double scaleincr = (kf2.pos.scale - kf1.pos.scale) / 1000;

	double timeincr = (kf2.ts.timeOs - kf1.ts.timeOs) / 1000;
	double timeSimincr = (kf2.ts.timeSim - kf1.ts.timeSim) / 1000;

	XYZ p1, p2;
	RThetaPhi a1, a2;
	p1.x = kf1.pos.xpos;
	p1.y = kf1.pos.ypos;
	p1.z = kf1.pos.zpos;
	p2.x = kf2.pos.xpos;
	p2.y = kf2.pos.ypos;
	p2.z = kf2.pos.zpos;
	a1 = toSpherical(p1);
	a2 = toSpherical(p2);
	double rincr = (a2.r - a1.r) / 1000;
	double thetaincr = (a2.theta - a1.theta) / 1000;
	double phiincr = (a2.phi - a1.phi) / 1000;

	// now loop through the intermediate points
	CameraKeyFrame ikf;
	XYZ p;
	RThetaPhi a;
	ikf.copyFrom(kf1);
	double quatmodulus;

	p.x = kf1.pos.xpos;
	p.y = kf1.pos.ypos;
	p.z = kf1.pos.zpos;
	a = toSpherical(p);
	
	for (int i = 1; i < 1000; i++) {
		// add suitable increment to interpolated CameraKeyFrame
		ikf.incrementAllTimestamps(timeincr, timeSimincr);
				
		a.r += rincr*(splinevals[i] - splinevals[i-1])/0.001;	
			// rincr multiplied by the ratio of the splinevals increment to linear increment, 
			// which is 0.001 in the range 0 to 1
		a.theta += thetaincr*(splinevals[i] - splinevals[i-1])/0.001;
		a.phi += phiincr*(splinevals[i] - splinevals[i-1])/0.001;
				
		p = toCartesian(a);
		ikf.pos.xpos = p.x;
		ikf.pos.ypos = p.y;
		ikf.pos.zpos = p.z;
		// ikf.pos.xrot += xrotincr; // this causes discontinuity at end due to normalization
		ikf.pos.xrot = i*xrotincr*(splinevals[i] - splinevals[0]) + kf1.pos.xrot;
		ikf.pos.yrot = i*yrotincr*(splinevals[i] - splinevals[0]) + kf1.pos.yrot;
		ikf.pos.zrot = i*zrotincr*(splinevals[i] - splinevals[0]) + kf1.pos.zrot;
		ikf.pos.wrot = i*wrotincr*(splinevals[i] - splinevals[0]) + kf1.pos.wrot;
		ikf.pos.scale += scaleincr*(splinevals[i] - splinevals[0]);
		// need to normalize the rot quat
		ikf.normalizeQuat();		
		destfileout << ikf.getCamkfAscii();
	}
	
}

CameraKeyFrame prevkf;

int main(int argc,char *argv[])
{
	std::cout << "This tool takes an initial osrectxt file, appends to it camera keyframes from subsequent osrectxt files with suitable interpolation, and saves as a new osrectxt file.";
	std::cout << std::endl << std::endl;
	std::cout << "https://github.com/OpenSpace/OpenSpace/discussions/3294" << std::endl;
	std::cout << "https://github.com/hn-88/openspace-scripts/wiki" << std::endl;
	std::cout << "https://docs.openspaceproject.com/en/releases-v0.20.0/content/session-recording.html#ascii-file-format" << std::endl;
	std::cout << std::endl;
	std::cout << "Documentation is at https://github.com/hn-88/OSREC-interp/wiki" << std::endl;

	tinyfd_messageBox("Please Note", 
			"First input the destination OSRECTXT file. This tool then takes an initial osrectxt file, appends to it camera keyframes from subsequent osrectxt files with suitable interpolation, and saves to the destination OSRECTXT file.", 
			"ok", "info", 1);
	
	char const * SaveFileName = "";
	char const * OpenFileName = "";
	char const * FilterPatterns[2] =  { "*.osrectxt","*.osrec" };

	SaveFileName = tinyfd_saveFileDialog(
		"Choose the name and path of the destination file",
		"final.osrectxt",
		2,
		FilterPatterns,
		NULL);
	try {
		destfileout.open(SaveFileName, std::ofstream::out );		
	} catch (int) {
		std::cerr << "An error occured creating destination file."<< std::endl ; 
		return false;
	}

	

	
	OpenFileName = tinyfd_openFileDialog(
				"Open the initial osrectxt file",
				"",
				2,
				FilterPatterns,
				NULL,
				0);
	//////////////////////////////////////////////////
	// from https://github.com/OpenSpace/OpenSpace/blob/0ff646a94c8505b2b285fdd51800cdebe0dda111/src/interaction/sessionrecording.cpp#L363
	// checking the osrectxt format etc
	/////////////////////////
	std::string _playbackFilename = OpenFileName;
	bool validf = checkIfValidRecFile(_playbackFilename);
	if (!validf) {
		return false;
	}
	
	//////////////////////////////////////////////////////
	// start the copy from initial osrectxt to destination
	// and also find the last camera keyframe.
	//////////////////////////////////////////////////////

	std::ifstream _playbackFile;
	_playbackFile.open(_playbackFilename, std::ifstream::in);
	char line[lineBufferForGetlineSize];
	while(_playbackFile.getline(line, lineBufferForGetlineSize)) {
		destfileout << line << std::endl;
		if(line[0] == 'c') {
			if(line[1] == 'a') {
				if(line[2] == 'm') {
					tempstring = line;
				}
			}
		}
		
	}
	// tempstring now contains the last camera keyframe
	prevkf.populateCamkfAscii(tempstring);

	bool appendAnother, ignoreTime;
	bool ignoreAll=false;
	bool keepSimTimeforAll=false;
	bool donotaskagain=false;
	while(true) {
		appendAnother = tinyfd_messageBox(
		"Append next keyframe osrectxt" , 
		"Append one more osrectxt?"  , 
		"yesno" , 
		"question" , 
		1 ) ;

		if (!appendAnother) {
			break;
		}
		// else, append the next keyframe, that is
		// the last camera keyframe of next osrectxt file
		CameraKeyFrame kf;
		char const * lTmp;
		lTmp = tinyfd_inputBox(
			"Please Input", "Desired playback time till next keyframe in seconds", "10.0");
			if (!lTmp) return 1 ;	
		timeincrstr =  lTmp;
		timeincr = atof(lTmp);
		if (ignoreAll) {
			ignoreTime = true;
		}			
		else {
			ignoreTime = false;
			if (!donotaskagain)
		ignoreTime = tinyfd_messageBox(
			"Ignore simulation time?" , 
			"Ignore the next keyframe's simulation time?"  , 
			"yesno" , 
			"question" , 
			1 ) ;
		useSpline = tinyfd_messageBox(
			"Ease in and out?" , 
			"Use a spline to ease in and out of keyframes?"  , 
			"yesno" , 
			"question" , 
			1 ) ;
		
		if(ignoreTime && !donotaskagain) {
			ignoreAll = tinyfd_messageBox("Ignore for all?", 
			"Ignore every keyframe's simulation time during this run? If yes, you won't be asked again. If not, you will be asked for each keyframe.", 
			"yesno", "question", 1);
			if (ignoreAll) donotaskagain = true;
			//script 956.698 0 768100268.890  1 openspace.time.setPause(true)
			std::string scriptstring = "1 openspace.time.setPause(true)";
			ScriptKeyFrame skf;
			skf.setTimestampsFrom(prevkf);
			skf.setScriptString(scriptstring);
			destfileout << skf.getScrkfAscii();
		} else {
			// do not ignore simu time
			if(!ignoreTime && !donotaskagain)
			keepSimTimeforAll = tinyfd_messageBox("Keep Sim Time for all?", 
			"Keep every keyframe's simulation time during this run? If yes, you won't be asked again. If not, you will be asked for each keyframe.", 
			"yesno", "question", 1);
			if (keepSimTimeforAll) {
				donotaskagain = true;				
			}

			std::string scriptstring = "1 openspace.time.setPause(false)";
			ScriptKeyFrame skf;
			skf.setTimestampsFrom(prevkf);
			skf.setScriptString(scriptstring);
			destfileout << skf.getScrkfAscii();			
		}
		}
		OpenFileName = tinyfd_openFileDialog(
				"Open the next osrectxt file",
				"",
				2,
				FilterPatterns,
				NULL,
				0);
		std::string pbFilename = OpenFileName;
		bool validf = checkIfValidRecFile(pbFilename);
		if (!validf) {
			tinyfd_messageBox("Sorry!", 
			"That doesn't seem to be a valid osrec file. Exiting.", 
			"ok", "info", 1);
			return false;
		}
		std::string nextKfstr = getLastCameraKfstring(pbFilename);
		kf.populateCamkfAscii(nextKfstr);
		// set timeOS & timeRec to previous keyframe's values
		kf.setTimestampsFrom(prevkf);
		if (!ignoreTime) {
			double timeRecdiff = timeincr;
			double timeSimdiff = kf.ts.timeSim - prevkf.ts.timeSim;
			int deltatime = (int)(timeSimdiff / timeRecdiff);
			std::string scriptstring = "1 openspace.time.setDeltaTime(" + std::to_string(deltatime) + ")";
			ScriptKeyFrame skf;
			skf.setTimestampsFrom(prevkf);
			skf.setScriptString(scriptstring);
			destfileout << skf.getScrkfAscii();
		}
		// increment timeOS & timeRec
		kf.incrementOnlyTwoTimestamps(timeincr);
		if(useSpline) {
			splineinterpolatebetween(prevkf, kf);
		}
		else {
			interpolatebetween(prevkf, kf); 
		}
		
		// write the kf out to destfile
		destfileout << kf.getCamkfAscii();
		
		// update prevkf
		prevkf.copyFrom(kf);
		
	} // end while loop for new keyframes
	   
} // end main
