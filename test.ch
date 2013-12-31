function gdc(a, b)
    var c = a;
    var d = b;

    while c != d do
        if c > d then
            c = c - d
        else
            d = d - c
        end
    end;

    c
end;

gdc(17 * 24, 17 * 25)
