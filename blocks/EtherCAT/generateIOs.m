function [pdo_info pdo_entry input output] = generateIOs(TxPdo,RxPdo);
pdo_info = [];
pdo_entry = [];

pdo_entry_idx = 1;
pdo_info_idx = 1;

output = [];
for i = 1:size(TxPdo,2)
    pdo_info(pdo_info_idx, :) = [ 0 TxPdo(i).index, ...
        pdo_entry_idx-1 size(TxPdo(i).entry,2)];
    for j = 1:size(TxPdo(i).entry,2)
        
        if TxPdo(i).entry(j).signed
            bitlen = -TxPdo(i).entry(j).bitlen;
        else
            bitlen = TxPdo(i).entry(j).bitlen;
        end
        pdo_entry(pdo_entry_idx, :) = [...
            TxPdo(i).entry(j).index, ...
            TxPdo(i).entry(j).subindex, ...
            bitlen, ...
            ];
        pdo_entry_idx = pdo_entry_idx + 1;
        
        if ~TxPdo(i).entry(j).index
            continue
        end

        output(size(output,2)+1).pdo_map = [pdo_info_idx-1 j-1];
        
    end
    pdo_info_idx = pdo_info_idx + 1;
end

input = [];
for i = 1:size(RxPdo,2)
    pdo_info(pdo_info_idx, :) = [ 1 RxPdo(i).index, ...
        pdo_entry_idx-1 size(RxPdo(i).entry,2)];
    for j = 1:size(RxPdo(i).entry,2)
        
        if RxPdo(i).entry(j).signed
            bitlen = -RxPdo(i).entry(j).bitlen;
        else
            bitlen = RxPdo(i).entry(j).bitlen;
        end
        pdo_entry(pdo_entry_idx, :) = [...
            RxPdo(i).entry(j).index, ...
            RxPdo(i).entry(j).subindex, ...
            bitlen, ...
            ];
        pdo_entry_idx = pdo_entry_idx + 1;
        
        if ~RxPdo(i).entry(j).index
            continue
        end

        input(size(input,2)+1).pdo_map = [pdo_info_idx-1 j-1];
        
    end
    pdo_info_idx = pdo_info_idx + 1;
end
