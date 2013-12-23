function fac(n)
    if n == 0 then
        1
    else
        fac(n - 1) * n
    end
end;

fac(5)
