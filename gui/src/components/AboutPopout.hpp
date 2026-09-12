#pragma once

class AboutPopout {
public:
    void open(void);

    void draw(void);

private:
    bool pending = false;
};
