function [PdoInfo PdoEntryInfo input output] = generateIOs(TxPdo,RxPdo)
    [output TxPdoInfo TxPdoEntryInfo ] = getIOSpec(TxPdo, 0);
    [ input RxPdoInfo RxPdoEntryInfo ] = getIOSpec(RxPdo, 1);
    
    % The first element of input(x).pdo_map points to a rows in
    % RxPdoInfo. Since RxPdoInfo is appended to TxPdoInfo to create
    % a joint PdoInfo, the element input(x).pdo_map(1) has to be 
    % incremented by the number of rows of TxPdoInfo
    TxPdoInfoLen = size(TxPdoInfo,1);
    for i = 1:size(input,2)
        input(i).pdo_map(1) = input(i).pdo_map(1) + TxPdoInfoLen;
    end
    
    % The third column of RxPdoInfo points to a row in RxPdoEntryInfo.
    % Since RxPdoEntryInfo is appended to TxPdoEntryInfo to create
    % a joint PdoEntryInfo, the element RxPdoInfo(x,3) has to be 
    % incremented by the row count of TxPdoEntryInfo
    TxPdoEntryInfoLen = size(TxPdoEntryInfo,1);
    if ~isempty(RxPdoInfo)
        RxPdoInfo(:,3) = RxPdoInfo(:,3) + TxPdoEntryInfoLen;
    end
    
    % Now create the joint PdoInfo and PdoEntryInfo
    PdoInfo = [TxPdoInfo; RxPdoInfo];
    PdoEntryInfo = [TxPdoEntryInfo; RxPdoEntryInfo];

function [io PdoInfo PdoEntryInfo] = getIOSpec(Pdo, dir)
    PdoInfo = [];
    PdoEntryInfo = [];
    io = [];
    pdo_info_idx = 1;
    pdo_entry_idx = 1;
    io_idx = 1;

    for i = 1:size(Pdo,2)
        PdoInfo(pdo_info_idx, :) = [ dir Pdo(i).index, ...
            pdo_entry_idx-1 size(Pdo(i).entry,2)];

        for j = 1:PdoInfo(pdo_info_idx,4)
            PdoEntryInfo(pdo_entry_idx, :) = [...
                Pdo(i).entry(j).index, ...
                Pdo(i).entry(j).subindex, ...
                Pdo(i).entry(j).bitlen, ...
                Pdo(i).entry(j).datatype, ...
                ];
            pdo_entry_idx = pdo_entry_idx + 1;

            if ~Pdo(i).entry(j).index
                continue
            end

            io(io_idx).pdo_map = [pdo_info_idx-1 j-1];
            io_idx = io_idx + 1;

        end
        pdo_info_idx = pdo_info_idx + 1;
    end
