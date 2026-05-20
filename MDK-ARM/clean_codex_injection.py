import os
import re

def clean_codex_injection(file_path):
    if not os.path.exists(file_path):
        print(f"错误: 找不到工程文件 {file_path}")
        return

    try:
        # 以 utf-8 模式读取源文件
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # 1. 精确匹配并删除 Codex 注入的 lvgl_sjpg 组及其内部所有内容
        # 使用正则表达式非贪婪匹配 <Group> 到 </Group>
        pattern = re.compile(r'<Group>\s*<GroupName>lvgl_sjpg</GroupName>.*?</Group>\s*', re.DOTALL)
        
        if pattern.search(content):
            cleaned_content = pattern.sub('', content)
            print("成功定位并切除了 Codex 注入的非法 lvgl_sjpg 节点。")
        else:
            cleaned_content = content
            print("未找到 lvgl_sjpg 节点，可能已被手动删除。")

        # 2. 清理 AI 注入可能带来的非标准空格 (不间断空格 U+00A0 替换为标准空格)
        cleaned_content = cleaned_content.replace('\u00A0', ' ')
        print("已完成底层空白字符规范化。")

        # 将修复后的干净内容写回
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(cleaned_content)
        
        print(f"修复大功告成！\n请重新双击打开 {os.path.basename(file_path)}")

    except Exception as e:
        print(f"修复过程中发生异常: {e}")

if __name__ == "__main__":
    # 请确认这里的路径是你报错工程的绝对路径
    target_project_file = r"D:\STDemoProject\Cooling_V2\MDK-ARM\Demo_F429_DDS.uvprojx"
    
    # 执行修复
    clean_codex_injection(target_project_file)