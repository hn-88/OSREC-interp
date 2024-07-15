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
// spline lookup table values computed with python code in Python directory
double splinevals[999] = {0.0,0.00010020523270353699,0.00020043943890144837,0.0003007315920881086,0.00040111066575789205,0.0005016056334051732,0.0006022454685243263,0.0007030591446097259,0.0008040756351557464,0.000905323913656762,0.0010068329536071475,0.0011086317285012772,0.001210749211833525,0.0013132143770982659,0.001416056197789874,0.0015193036474027242,0.00162298569943119,0.0017271313273696473,0.0018317695047124687,0.0019369292049540297,0.002042639401588704,0.0021489290681108667,0.0022558271780148925,0.002363362704795155,0.002471564621946029,0.002580461902961888,0.002690083521337108,0.0028004584505660624,0.002911615664143126,0.0030235841355626732,0.003136392838319078,0.003250070745906715,0.003364646831819958,0.003480150069553183,0.003596609432600764,0.0037140538944570745,0.0038325124286164892,0.003952014008573383,0.0040725876078221285,0.004194262199857103,0.004317066758172678,0.0044410302562632314,0.004566181667623135,0.004692549965746763,0.0048201641241284905,0.004949053116262692,0.005079245915643741,0.0052107714957660145,0.005343658830123884,0.005477936892211726,0.005613634655523914,0.0057507810935548236,0.005889405179798825,0.006029535887750298,0.006171202190903613,0.006314433062753148,0.006459257476793274,0.006605704406518368,0.006753802825422803,0.006903581707000953,0.007055070024747194,0.007208296752155898,0.007363290862721443,0.0075200813299382,0.007678697127300543,0.007839167228302852,0.008001520606439496,0.008165786235204852,0.00833199308809329,0.00850017013859919,0.008670346360216925,0.008842550726440869,0.009016812210765394,0.009193159786684878,0.009371622427693693,0.009552229107286214,0.009735008798956817,0.009919990476199873,0.010107203112509757,0.010296675681380851,0.010488437156307519,0.01068251651078414,0.01087894271830509,0.011077744752364742,0.011278951586457467,0.011482592194077643,0.011688695548719645,0.011897290623877847,0.012108406393046618,0.012322071829720341,0.012538315907393386,0.012757167599560127,0.012978655879714938,0.013202809721352196,0.013429658097966275,0.013659229983051548,0.013891554350102387,0.014126660172613173,0.014364576424078273,0.01460533207799207,0.014848956107848928,0.015095477487143232,0.015344925189369349,0.015597328188021654,0.015852715456594522,0.01611111596858233,0.01637255869747945,0.01663707261678026,0.016904686699979132,0.017175429920570442,0.01744933125204856,0.01772641966790786,0.018006724141642727,0.01829027364674752,0.018577097156716627,0.018867223645044413,0.019160682085225256,0.019457501450753534,0.019757710715123607,0.02006133885182987,0.020368414834366685,0.020678967636228427,0.020993026230909477,0.021310619591904206,0.021631776692706987,0.021956526506812186,0.02228489800771419,0.022616920168907373,0.02295262196388611,0.023292032366144765,0.02363518034917772,0.02398209488647935,0.024332804951544024,0.02468733951786612,0.025045727558940016,0.025407998048260078,0.02577417995932069,0.026144302265616218,0.026518393940641052,0.026896483957889534,0.027278601290856068,0.027664774913035022,0.028055033797920768,0.028449406919007672,0.028847923249790127,0.029250611763762487,0.029657501434419142,0.030068621235254465,0.030484000139762824,0.03090366712143859,0.03132765115377614,0.03175598121026987,0.03218868626441412,0.03262579528970327,0.033067337259631714,0.03351334114769382,0.03396383592738396,0.03441885057219651,0.03487841405562584,0.03534255535116632,0.03581130343231234,0.03628468727255827,0.03676273584539847,0.037245478124327325,0.03773294308283921,0.0382251596944285,0.03872215693258956,0.039223963770816786,0.03973060918260452,0.040242122141447165,0.04075853162083909,0.04127986659427466,0.04180615603524825,0.04233742891725425,0.04287371421378701,0.04341504089834092,0.043961437944410355,0.04451293432548969,0.045069559015073285,0.04563134098665553,0.04619830921373079,0.04677049266979343,0.04734792032833786,0.04793062116285842,0.04851862414684951,0.04911195825380549,0.04971065245722072,0.0503147357305896,0.0509242370474065,0.05153918538116578,0.052159609705361834,0.05278553899348903,0.05341700221904171,0.054054028355514296,0.05469664637640115,0.055344885255196624,0.055998773965395106,0.056658341480490995,0.05732361677397862,0.05799462881935238,0.05867140659010667,0.05935397905973581,0.06004237520173423,0.06073662398959627,0.06143675439681632,0.062142795396888745,0.06285477596330791,0.06357272506956824,0.06429667168916402,0.0650266447955897,0.06576267336233965,0.0665047863629082,0.06725301277078977,0.06800738155947872,0.06876792170246941,0.06953466217325623,0.07030763194533353,0.0710868599921957,0.07187237528733714,0.07266420680425219,0.07346238351643523,0.07426693439738065,0.07507788842058281,0.07589527455953607,0.07671912178773485,0.0775494590786735,0.07838631540584637,0.07922971974274788,0.08007970106287236,0.08093628833971421,0.08179951054676782,0.08266939665752752,0.08354597564548771,0.08442927648414277,0.08531932814698706,0.08621615960751497,0.08711979983922086,0.08803027781559909,0.08894762251014411,0.08987186289635017,0.09080302794771178,0.0917411466377232,0.0926862479398789,0.09363836082767317,0.09459751427460042,0.09556373725415504,0.0965370587398314,0.09751750770512385,0.09850511312352676,0.09949990396853456,0.10050190800641261,0.10151112523716098,0.10252752789451422,0.10355108700497791,0.10458177359505773,0.10561955869125927,0.10666441332008816,0.10771630850805004,0.10877521528165053,0.10984110466739526,0.11091394769178982,0.11199371538133988,0.11308037876255106,0.11417390886192898,0.11527427670597923,0.11638145332120747,0.11749540973411934,0.11861611697122046,0.11974354605901641,0.12087766802401287,0.12201845389271541,0.12316587469162972,0.1243199014472614,0.12548050518611606,0.12664765693469937,0.12782132771951685,0.12900148856707425,0.13018811050387713,0.13138116455643115,0.13258062175124188,0.133786453114815,0.13499862967365608,0.1362171224542708,0.1374419024831648,0.13867294078684364,0.139910208391813,0.14115367632457845,0.14240331561164568,0.14365909727952028,0.1449209923547079,0.1461889718637141,0.14746300683304459,0.14874306828920492,0.15002912725870077,0.15132115476803773,0.15261912184372148,0.1539229995122576,0.1552327588001517,0.15654837073390945,0.15786980634003644,0.15919703664503834,0.16053003267542074,0.16186876545768927,0.16321320601834954,0.16456332538390722,0.16591909458086782,0.16728048463573714,0.16864746657502072,0.17002001142522416,0.17139809021285315,0.17278167396441324,0.17417073370641012,0.17556524046534938,0.17696516526773667,0.1783704791400776,0.1797811531088778,0.18119715820064286,0.18261846544187843,0.1840450458590902,0.18547687047878367,0.18691391032746457,0.18835613643163848,0.18980351981781102,0.19125603151248788,0.1927136425421746,0.19417632393337686,0.19564404671260027,0.1971167819063504,0.19859450054113298,0.20007717364345357,0.2015647722398178,0.2030572673567313,0.20455463002069973,0.20605683125822866,0.20756384209582374,0.20907563355999065,0.2105921766772349,0.21211344247406222,0.21363940197697823,0.21517002621248837,0.21670528620709856,0.21824515298731423,0.21978959757964106,0.22133859101058465,0.22289210430665074,0.22445010849434477,0.22601257460017243,0.2275794736506394,0.2291507766722513,0.23072645469151382,0.23230647873493243,0.23389081982901277,0.23547944900026058,0.23707233727518146,0.23866945568028097,0.24027077524206475,0.24187626698703843,0.24348590194170766,0.2450996511325781,0.24671748558615533,0.24833937632894496,0.24996529438745252,0.25159521078818387,0.2532290965576445,0.25486692272234,0.2565086603087761,0.2581542803434584,0.25980375385289245,0.2614570518635839,0.2631141454020385,0.2647750054947617,0.26643960316825915,0.26810790944903656,0.2697798953635996,0.27145553193845373,0.27313479020010467,0.2748176411750581,0.27650405588981947,0.27819400537089456,0.279887460644789,0.2815843927380084,0.28328477267705826,0.28498857148844436,0.2866957601986722,0.28840630983424753,0.2901201914216759,0.29183737598746295,0.2935578345581143,0.2952815381601356,0.2970084578200325,0.29873856456431047,0.30047182941947537,0.3022082234120327,0.303947717568488,0.30569028291534706,0.30743589047911535,0.30918451128629865,0.31093611636340257,0.31269067673693257,0.31444816343339443,0.3162085474792937,0.3179717999011361,0.31973789172542716,0.32150679397867254,0.3232784776873779,0.32505291387804874,0.3268300735771908,0.3286099278113097,0.330392447606911,0.33217760399050045,0.3339653679885836,0.335755710627666,0.33754860293425343,0.33934401593485136,0.34114192065596555,0.3429422881241015,0.3447450893657648,0.34655029540746135,0.34835787727569656,0.35016780599697617,0.3519800525978056,0.35379458810469067,0.35561138354413696,0.3574304099426501,0.3592516383267356,0.3610750397228992,0.36290058515764656,0.36472824565748324,0.3665579922489149,0.3683897959584471,0.37022362781258555,0.3720594588378358,0.37389726006070345,0.3757370025076943,0.3775786572053138,0.37942219518006765,0.38126758745846145,0.38311480506700085,0.3849638190321915,0.38681460038053894,0.38866712013854887,0.39052134933272686,0.3923772589895786,0.39423482013560973,0.3960940037973257,0.3979547810012323,0.3998171227738352,0.4016810001416399,0.403546384131152,0.4054132457688772,0.4072815560813212,0.4091512860949895,0.4110224068363878,0.41289488933202173,0.4147687046083968,0.41664382369201874,0.41852021760939323,0.4203978573870258,0.42227671405142203,0.42415675862908764,0.4260379621465282,0.42792029563024936,0.42980373010675677,0.431688236602556,0.4335737861441528,0.43546034975805264,0.43734789847076133,0.4392364033087841,0.44112583529862703,0.4430161654667957,0.44490736483979537,0.44679940444413196,0.4486922553063109,0.45058588845283815,0.4524802749102191,0.4543753857049594,0.4562711918635647,0.45816766441254064,0.46006477437839266,0.46196249278762674,0.4638607906667481,0.4657596390422628,0.4676590089406762,0.46955887138849384,0.4714591974122216,0.4733599580383647,0.4752611242934294,0.4771626672039208,0.4790645577963447,0.48096676709720676,0.48286926613301245,0.4847720259302677,0.4866750175154778,0.48857821191514855,0.4904815801557856,0.49238509326389446,0.49428872226598086,0.4961924381885503,0.4980962120581086,0.5000000149011612,0.5019038177442137,0.5038075916137721,0.5057113075363414,0.5076149365384279,0.5095184496465367,0.5114218178871738,0.5133250122868445,0.5152280038720547,0.5171307636693097,0.5190332627051156,0.5209354720059776,0.5228373625984015,0.5247389055088929,0.5266400717639576,0.5285408323901007,0.5304411584138286,0.5323410208616461,0.5342403907600596,0.536139239135574,0.5380375370146957,0.5399352554239295,0.5418323653897817,0.5437288379387575,0.545624644097363,0.5475197548921031,0.5494141413494842,0.5513077744960113,0.5532006253581905,0.555092664962527,0.5569838643355269,0.5588741945036952,0.5607636264935383,0.5626521313315611,0.5645396800442698,0.5664262436581696,0.5683117931997664,0.5701962996955656,0.5720797341720731,0.573962067655794,0.5758432711732349,0.5777233157509003,0.5796021724152968,0.5814798121929292,0.5833562061103037,0.5852313251939255,0.5871051404703007,0.5889776229659345,0.5908487437073329,0.5927184737210011,0.5945867840334451,0.5964536456711705,0.5983190296606826,0.6001829070284873,0.6020452488010901,0.6039060260049968,0.6057652096667128,0.6076227708127437,0.6094786804695955,0.6113329096637736,0.6131854294217833,0.615036210770131,0.6168852247353215,0.6187324423438609,0.6205778346222547,0.6224213725970086,0.6242630272946281,0.626102769741619,0.6279405709644866,0.629776401989737,0.6316102338438753,0.6334420375534076,0.6352717841448391,0.6370994446446758,0.6389249900794232,0.6407483914755869,0.6425696198596723,0.6443886462581856,0.6462054416976317,0.648019977204517,0.6498322238053462,0.6516421525266258,0.653449734394861,0.6552549404365577,0.6570577416782208,0.658858109146357,0.6606560138674711,0.6624514268680691,0.6642443191746564,0.6660346618137389,0.6678224258118219,0.6696075821954115,0.6713901019910127,0.6731699562251318,0.6749471159242737,0.6767215521149448,0.67849323582365,0.6802621380768954,0.6820282299011864,0.6837914823230289,0.685551866368928,0.68730935306539,0.6890639134389199,0.6908155185160239,0.6925641393232072,0.6943097468869756,0.6960523122338345,0.6977918063902899,0.6995282003828471,0.701261465238012,0.7029915719822899,0.7047184916421868,0.7064421952442081,0.7081626538148593,0.7098798383806465,0.7115937199680749,0.7133042696036502,0.715011458313878,0.7167152571252642,0.718415637064314,0.7201125691575334,0.7218060244314277,0.7234959739125029,0.7251823886272644,0.7268652396022177,0.7285444978638685,0.7302201344387229,0.7318921203532858,0.7335604266340633,0.7352250243075606,0.7368858844002839,0.7385429779387385,0.7401962759494299,0.741845749458864,0.7434913694935463,0.7451331070799824,0.7467709332446779,0.7484048190141384,0.75003473541487,0.7516606534733776,0.7532825442161671,0.7549003786697442,0.7565141278606149,0.758123762815284,0.7597292545602579,0.7613305741220415,0.7629276925271412,0.7645205808020619,0.7661092099733097,0.7676935510673901,0.7692735751108088,0.7708492531300711,0.7724205561516831,0.77398745520215,0.7755499213079778,0.7771079254956719,0.7786614387917379,0.7802104322226815,0.7817548768150082,0.7832947435952238,0.7848300035898341,0.7863606278253444,0.7878865873282603,0.7894078531250875,0.7909243962423319,0.7924361877064987,0.7939431985440939,0.7954453997816227,0.796942762445591,0.7984352575625048,0.7999228561588688,0.8014055292611896,0.8028832478959719,0.8043559830897222,0.8058237058689456,0.8072863872601479,0.8087439982898346,0.8101965099845115,0.811643893370684,0.813086119474858,0.8145231593235387,0.8159549839432323,0.817381564360444,0.8188028716016796,0.8202188766934446,0.821629550662245,0.8230348645345857,0.8244347893369731,0.8258292960959123,0.8272183558379093,0.8286019395894694,0.8299800183770983,0.8313525632273016,0.8327195451665854,0.8340809352214547,0.8354367044184153,0.836786823783973,0.8381312643446333,0.8394699971269018,0.8408029931572842,0.8421302234622859,0.843451659068413,0.8447672710021707,0.8460770302900651,0.8473809079586009,0.8486788750342849,0.8499709025436217,0.8512569615131176,0.852537022969278,0.8538110579386086,0.8550790374476145,0.8563409325228022,0.8575967141906768,0.8588463534777442,0.8600898214105095,0.8613270890154789,0.8625581273191577,0.8637829073480516,0.8650014001286664,0.8662135766875075,0.8674194080510806,0.8686188652458915,0.8698119192984454,0.8709985412352482,0.8721787020828057,0.8733523728676231,0.8745195246162063,0.875680128355061,0.8768341551106928,0.877981575909607,0.8791223617783095,0.880256483743306,0.8813839128311021,0.8825046200682031,0.8836185764811152,0.8847257530963433,0.8858261209403935,0.8869196510397713,0.8880063144209828,0.8890860821105326,0.8901589251349273,0.8912248145206718,0.8922837212942726,0.8933356164822343,0.8943804711110632,0.8954182562072648,0.8964489427973446,0.8974725019078083,0.8984889045651615,0.8994981217959098,0.9005001258337878,0.9014949166787891,0.9024825220971686,0.9034629710624087,0.9044362925479925,0.9054025155274028,0.9063616689741224,0.9073137818616341,0.908258883163421,0.9091970018529656,0.9101281669037511,0.9110524072892601,0.9119697519829757,0.9128802299583806,0.9137838701889577,0.9146807016481899,0.9155707533095602,0.9164540541465511,0.9173306331326457,0.9182005192413268,0.9190637414460774,0.9199203287203799,0.920770310037718,0.9216137143715737,0.9224505706954305,0.9232809079827706,0.9241047552070776,0.9249221413418338,0.9257330953605224,0.9265376462366263,0.9273358229436278,0.9281276544550104,0.9289131697442566,0.9296923977848495,0.9304653675502716,0.9312321080140064,0.9319926481495361,0.932747016930344,0.9334952433299125,0.9342373563217252,0.9349733848792641,0.9357033579760126,0.9364273045854533,0.9371452536810695,0.9378572342363435,0.9385632752247586,0.9392634056197974,0.9399576543949427,0.9406460505236777,0.9413286229794849,0.9420054007358474,0.942676412766248,0.9433416880441696,0.944001255543095,0.9446551442365069,0.9453033830978885,0.9459460011007224,0.9465830272184916,0.9472144904246789,0.9478404196927671,0.9484608439962392,0.9490757923085782,0.9496852936032663,0.9502893768537871,0.9508880710336233,0.9514814051162573,0.9520694080751727,0.9526521088838519,0.9532295365157777,0.953801719944433,0.9543686881433009,0.9549304700858641,0.9554870947456053,0.9560385910960078,0.9565849881105539,0.9571263147627271,0.9576626000260098,0.9581938728738848,0.9587201622798354,0.959241497217344,0.9597579066598938,0.9602694195809673,0.960776064954048,0.9612778717526179,0.9617748689501606,0.9622670855201585,0.9627545504360948,0.9632372926714521,0.9637153411997132,0.9641887249943613,0.9646574730288793,0.9651216142767494,0.9655811777114551,0.9660361923064791,0.9664866870353042,0.9669326908714132,0.967374232788289,0.9678113417594147,0.9682440467582729,0.9686723767583465,0.9690963607331184,0.9695160276560715,0.9699314065006888,0.9703425262404526,0.9707494158488463,0.9711521042993526,0.9715506205654544,0.9719449936206344,0.9723352524383756,0.972721425992161,0.9731035432554731,0.9734816332017951,0.9738557248046098,0.9742258470373999,0.9745920288736484,0.9749542992868381,0.9753126872504518,0.9756672217379725,0.9760179317228829,0.9763648461786659,0.9767079940788046,0.9770474043967816,0.9773831061060798,0.9777151281801822,0.9780434995925718,0.9783682493167308,0.9786894063261427,0.97900699959429,0.9793210580946559,0.979631610800723,0.9799386866859743,0.9802423147238923,0.9805425238879606,0.9808393431516612,0.9811328014884775,0.9814229278718924,0.9817097512753885,0.9819933006724488,0.982273605036556,0.9825506933411932,0.9828245945598431,0.9830953376659887,0.9833629516331126,0.9836274654346981,0.9838889080442275,0.9841473084351842,0.9844026955810506,0.9846550984553099,0.9849045460314447,0.9851510672829383,0.9853946911832729,0.9856354467059321,0.9858733628243981,0.986108468512154,0.9863407927426828,0.9865703644894674,0.9867972127259904,0.987021366425735,0.9872428545621836,0.9874617061088193,0.9876779500391251,0.9878916153265838,0.9881027309446779,0.9883113258668907,0.9885174290667051,0.9887210695176036,0.9889222761930692,0.989121078066585,0.9893175041116335,0.9895115833016978,0.9897033446102608,0.989892817010805,0.9900800294768137,0.9902650109817694,0.9904477904991553,0.9906283970024541,0.9908068594651486,0.9909832068607218,0.9911574681626563,0.9913296723444351,0.9914998483795414,0.9916680252414576,0.9918342319036668,0.9919984973396516,0.9921608505228952,0.9923213204268803,0.9924799360250898,0.9926367262910063,0.9927917201981131,0.9929449467198928,0.9930964348298283,0.9932462135014026,0.9933943117080983,0.9935407584233985,0.9936855826207859,0.9938288132737434,0.993970479355754,0.9941106098403002,0.9942492337008654,0.9943863799109319,0.994522077443983,0.9946563552735014,0.99478924237297,0.9949207677158715,0.995050960275689,0.995179849025905,0.9953074629400027,0.995433830991465,0.9955589821537745,0.9956829454004141,0.9958057497048668,0.9959274240406155,0.996047997381143,0.996167498699932,0.9962859569704655,0.9964034011662264,0.9965198602606975,0.9966353632273616,0.9967499390397018,0.9968636166712008,0.9969764250953412,0.9970883932856063,0.9971995502154788,0.9973099248584414,0.9974195461879771,0.997528443177569,0.9976366448006996,0.9977441800308517,0.9978510778415086,0.9979573672061529,0.9980630770982674,0.9981682364913349,0.9982728743588386,0.9983770196742611,0.9984807014110854,0.9985839485427941,0.9986867900428703,0.9987892548847968,0.9988913720420565,0.9989931704881322,0.9990946791965067,0.9991959271406631,0.999296943294084,0.9993977566302524,0.9994983961226511,0.9995988907447632,0.9996992694700712,0.999799561272058,0.9998997951242067,1.0};
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
		quatmodulus = std::sqrt(ikf.pos.xrot*ikf.pos.xrot + ikf.pos.yrot*ikf.pos.yrot + ikf.pos.zrot*ikf.pos.zrot + ikf.pos.wrot*ikf.pos.wrot);
		ikf.pos.xrot /= quatmodulus;
		ikf.pos.yrot /= quatmodulus;
		ikf.pos.zrot /= quatmodulus;
		ikf.pos.wrot /= quatmodulus;
		
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
	for (int i = 1; i < 1000; i++) {
		// add suitable increment to interpolated CameraKeyFrame
		ikf.incrementAllTimestamps(timeincr, timeSimincr);
		
		p.x = ikf.pos.xpos;
		p.y = ikf.pos.ypos;
		p.z = ikf.pos.zpos;
		a = toSpherical(p);
		a.r += rincr*splinevals[i-1];
		a.theta += thetaincr*splinevals[i-1];
		a.phi += phiincr*splinevals[i-1];
		p = toCartesian(a);
		ikf.pos.xpos = p.x;
		ikf.pos.ypos = p.y;
		ikf.pos.zpos = p.z;
		// ikf.pos.xrot += xrotincr; // this causes discontinuity at end due to normalization
		ikf.pos.xrot = i*xrotincr*splinevals[i-1] + kf1.pos.xrot;
		ikf.pos.yrot = i*yrotincr*splinevals[i-1] + kf1.pos.yrot;
		ikf.pos.zrot = i*zrotincr*splinevals[i-1] + kf1.pos.zrot;
		ikf.pos.wrot = i*wrotincr*splinevals[i-1] + kf1.pos.wrot;
		ikf.pos.scale += scaleincr*splinevals[i-1];
		// need to normalize the rot quat
		quatmodulus = std::sqrt(ikf.pos.xrot*ikf.pos.xrot + ikf.pos.yrot*ikf.pos.yrot + ikf.pos.zrot*ikf.pos.zrot + ikf.pos.wrot*ikf.pos.wrot);
		ikf.pos.xrot /= quatmodulus;
		ikf.pos.yrot /= quatmodulus;
		ikf.pos.zrot /= quatmodulus;
		ikf.pos.wrot /= quatmodulus;
		
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
