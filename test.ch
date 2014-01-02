var gcd = function (a, b)
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

gcd(17 * 2, 17 * 3)
