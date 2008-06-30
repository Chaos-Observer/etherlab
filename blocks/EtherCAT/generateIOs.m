function Pdo = generateIOs(EtherCATInfo,MapType)
    Pdo = {};
    pdo_index = 1;
    
    if ~isstruct(EtherCATInfo) || ~isfield(EtherCATInfo,'SyncManager')
        return
    end

    for i = 1:size(EtherCATInfo.SyncManager,2)
        if ~isfield(EtherCATInfo.SyncManager{i},'Pdo')
            continue
        end
        sm = EtherCATInfo.SyncManager{i};
        
        for j = 1:size(sm.Pdo,2)
            if ~isfield(sm.Pdo(j),'Entry')
                continue
            end
            for k = 1:size(sm.Pdo(j).Entry,2)
                pdo_entry = sm.Pdo(j).Entry(k);
                if ~pdo_entry.Index
                    continue
                end
                switch MapType
                    case 0
                        Pdo{pdo_index} = [pdo_entry.Index pdo_entry.SubIndex];
                        pdo_index = pdo_index + 1;
                end
            end
        end
    end
