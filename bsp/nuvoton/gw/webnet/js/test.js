

function addToast(msg, timeout = 1000) {
    $.Toast("Notice", msg, "success", {
        stack: false,
        has_icon: true,
        has_close_btn: true,
        fullscreen: true,
        timeout: timeout,
        sticky: false,
        has_progress: true,
        rtl: false,
    });
}


// function load_config() {
//     return new Promise((resolve, reject) => {

//         $.get("./config.json", function (response) {
//             console.log('2准备保存配置文件');
//             localStorage.setItem('config', JSON.stringify(response));
//             resolve(response)
//         })
//     }).then((response) => {
//         config = response
//         console.log('config', config)
//     })
// }

async function load_config(src = 'api') {

    const response_1 = await new Promise((resolve, reject) => {
        if (src !== 'api') {
            // 从文件系统中的配置文件获取
            console.log('从config.json获取配置', src)
            $.get("./config.json", function (response) {
                console.log('async load config from config.json');
                localStorage.setItem('config', JSON.stringify(response));
                resolve(response);
            });
        } else {
            // 从 API 中获取
            console.log('从api获取配置', src)
            $.get("cgi-bin/get_config", function (response) {
                console.log('async load config from api get_config');
                localStorage.setItem('config', response);
                resolve(JSON.parse(response));
            });
        }
    });
    config = response_1;
    console.log('config', config);
}

async function put_config(config) {
    return await new Promise((resolve, reject) => {
        console.log("put_config 开始更新配置")
        $.post("cgi-bin/put_config", config, (res) => {
            console.log('put_config res:', res)
            resolve(res)
        })
    })
}

async function chg_root(dir) {
    return await new Promise((resolve, reject) => {
        console.log("切换 webroot")
        $.post("cgi-bin/chg_root", { "path": dir }, (data, status) => {
            console.log('chg_root res:', status, data)
            if (status === 'success') {
                resolve(data)
            } else {
                reject(status)
            }
        })
    })
}

async function api_reset() {
    return await new Promise((resolve, reject) => {
        console.log("重启网关")
        $.post("cgi-bin/reset", (data, status) => {
            console.log('chg_root res:', status, data)
            if (status === 'success') {
                resolve(data)
            } else {
                reject(status)
            }
        })
    })
}

async function api_clear() {
    return await new Promise((resolve, reject) => {
        console.log("清空 webnet")
        $.post("cgi-bin/clear", (data, status) => {
            console.log('chg_root res:', status, data)
            if (status === 'success') {
                resolve(data)
            } else {
                reject(status)
            }
        })
    })
}

