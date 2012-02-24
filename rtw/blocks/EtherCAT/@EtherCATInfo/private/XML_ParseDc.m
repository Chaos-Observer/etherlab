function dc = XML_ParseDc(xml)
% Parses the <Dc> Element of xml doc
% Returns: struct in principle reflecting the contents of the <Dc> elemen

% Find the <Dc> element - minOccurs = 0, maxOccurs = 1
dcElem = xml.getElementsByTagName('Dc').item(0);
dc.OpMode = XML_ParseOpMode(dcElem);

%% --------------------------------------------------------------------------------
function opmode = XML_ParseOpMode(xml)

if isempty(xml)
    opmode = [];
    return
end

% Go through all <OpMode> elements; minOccurs = 0, maxOccurs = unbounded
opModeElements = xml.getElementsByTagName('OpMode');
for i = 1:opModeElements.getLength
    opmode(i) = XML_ParseOpModeItem(opModeElements.item(i-1));
end

%% --------------------------------------------------------------------------------
function opmodeItem = XML_ParseOpModeItem(xml)

if ~nargin
    opmodeItem = [];
    return
end

name = xml.getElementsByTagName('Name').item(0);
if isempty(name)
    opmodeItem.Name = [];
else
    opmodeItem.Name = char(name.getTextContent.trim);
end

desc = xml.getElementsByTagName('Desc').item(0);
if isempty(desc)
    opmodeItem.Desc = [];
else
    opmodeItem.Desc = char(desc.getTextContent.trim);
end

assignActivateElem = xml.getElementsByTagName('AssignActivate').item(0);
if isempty(assignActivateElem)
    InvalidDoc(xml,'XML_ParseOpModeItem','ElementNotFound','AssignActivate');
end

opmodeItem.AssignActivate = readHexDecValue(assignActivateElem,0);

% Get the timing information; minOccurs = 0, maxOccurs = 1
opmodeItem.CycleTimeSync0 = XML_ParseOpModeTime('CycleTimeSync0', xml);
opmodeItem.ShiftTimeSync0 = XML_ParseOpModeTime('ShiftTimeSync0', xml);
opmodeItem.CycleTimeSync1 = XML_ParseOpModeTime('CycleTimeSync1', xml);
opmodeItem.ShiftTimeSync1 = XML_ParseOpModeTime('ShiftTimeSync1', xml);

% SyncManager data
opmodeItem.Sm = XML_ParseOpModeSm(xml);


%% --------------------------------------------------------------------------------
function cts = XML_ParseOpModeTime(elemName, xml)

cts = [];

ctsElem = xml.getElementsByTagName(elemName).item(0);
if isempty(ctsElem)
    return
end

% Attribute Factor is optional
cts.Factor = readNumericValue(ctsElem,1,'Factor');
cts.Value = readNumericValue(ctsElem,0);

% Attribute 'Input' is really only specified for ShiftTimeSync0
if strcmp('ShiftTimeSync0',elemName)
    cts.Input = readBooleanValue(ctsElem,'Input');
end

%% --------------------------------------------------------------------------------
function sm = XML_ParseOpModeSm(xml)

smElems = xml.getElementsByTagName('Sm');

if ~smElems.getLength
    sm = [];
    return
end

for i = 1:smElems.getLength
    sm(i) = XML_ParseOpModeSmItem(smElems.item(i-1));
end

%% --------------------------------------------------------------------------------
function sme = XML_ParseOpModeSmItem(xml)

sme.No = readNumericValue(xml,0,'No');

pdoElems = xml.getElementsByTagName('Pdo');
sme.Pdo = repmat(XML_ParseOpModeSmPdo,1,pdoElems.getLength);
for i = 1:pdoElems.getLength
    sme.Pdo(i) = XML_ParseOpModeSmPdo(pdoElems.item(i-1));
end

%% --------------------------------------------------------------------------------
function pdo = XML_ParseOpModeSmPdo(xml)

pdo = struct('OSFac',[],'Value',[]);

if ~nargin
    return
end

% Attribute OSFac is optional
pdo.OSFac = readNumericValue(xml,1,'OSFac');
pdo.Value = readHexDecValue(xml,0);
