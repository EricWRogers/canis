#include <Canis/Profiler.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <numeric>

namespace Canis::Profiler
{
    namespace
    {
        const ImU32 colors[] = { IM_COL32(123,132,143,255), IM_COL32(91,194,126,255),
            IM_COL32(235,170,71,255), IM_COL32(102,155,239,255), IM_COL32(184,128,219,255),
            IM_COL32(65,201,192,255), IM_COL32(217,117,147,255), IM_COL32(170,171,110,255) };
        struct Row { std::string name; Category category; int parent; double total=0,self=0; int calls=0; };
        void Tip(const char* text) { if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",text); }
    }
    void DrawPanel()
    {
        static uint64_t selected = 0;
        static float zoom = 1;
        static std::string search, exportPath = "artifacts/profiler/capture.json", exportStatus;
        auto& recorder = Get();
        bool recording = recorder.IsRecording();
        if (ImGui::Checkbox("Record", &recording)) recorder.SetRecording(recording);
        ImGui::SameLine();
        bool live = selected == 0;
        if (ImGui::Checkbox("Live", &live)) selected = live || recorder.Frames().empty() ? 0 : recorder.Frames().back().id;
        ImGui::SameLine();
        if (ImGui::Button("Clear")) { recorder.Clear(); selected = 0; }
        ImGui::SameLine();
        if (ImGui::Button("Export")) ImGui::OpenPopup("Export CPU Capture");
        if (ImGui::BeginPopup("Export CPU Capture")) {
            ImGui::SetNextItemWidth(300); ImGui::InputText("Path", &exportPath);
            if (ImGui::Button("Export trace")) exportStatus = recorder.Export(exportPath) ? "Capture exported." : "Export failed.";
            if (!exportStatus.empty()) ImGui::TextUnformatted(exportStatus.c_str());
            ImGui::EndPopup();
        }
        const auto& frames = recorder.Frames();
        if (frames.empty()) { ImGui::TextDisabled("No captured frames"); return; }
        auto selectedIt = std::find_if(frames.begin(),frames.end(),[&](const Frame& frame){return frame.id==selected;});
        if (selected && selectedIt==frames.end()) { selected = frames.front().id; selectedIt = frames.begin(); }
        int index = selected ? static_cast<int>(selectedIt-frames.begin()) : static_cast<int>(frames.size()-1);
        if (ImGui::ArrowButton("Previous frame", ImGuiDir_Left)) selected = frames[std::max(0,index-1)].id;
        Tip("Previous frame"); ImGui::SameLine();
        if (ImGui::ArrowButton("Next frame", ImGuiDir_Right)) selected = frames[std::min(static_cast<int>(frames.size()-1),index+1)].id;
        Tip("Next frame"); ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(80.f,ImGui::GetContentRegionAvail().x));
        if (ImGui::SliderInt("##Frame", &index,0,static_cast<int>(frames.size()-1),"Frame index %d")) selected = frames[index].id;

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float width = std::max(1.f,ImGui::GetContentRegionAvail().x), height=140;
        ImGui::InvisibleButton("CPU frame history",ImVec2(width,height));
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin,ImVec2(origin.x+width,origin.y+height),IM_COL32(24,26,30,255));
        double ceiling = 33.333;
        for (const auto& frame : frames) ceiling=std::max(ceiling,frame.totalMs*1.08);
        const float barWidth=width/static_cast<float>(frames.size());
        for (size_t i=0;i<frames.size();++i) {
            float bottom=origin.y+height;
            for (size_t category=0;category<CategoryCount;++category) {
                const float h=static_cast<float>(frames[i].categories[category]/ceiling)*height;
                draw->AddRectFilled(ImVec2(origin.x+i*barWidth,bottom-h),ImVec2(origin.x+(i+1)*barWidth,bottom),colors[category]);
                bottom-=h;
            }
            if (frames[i].id==(selected ? selected : frames.back().id))
                draw->AddLine(ImVec2(origin.x+(i+.5f)*barWidth,origin.y),ImVec2(origin.x+(i+.5f)*barWidth,origin.y+height),IM_COL32_WHITE,2);
        }
        for (double budget : {16.667,33.333}) {
            float y=origin.y+height-static_cast<float>(budget/ceiling)*height;
            draw->AddLine(ImVec2(origin.x,y),ImVec2(origin.x+width,y),IM_COL32(240,240,240,90));
            char label[32]; snprintf(label,sizeof(label),"%.1f ms",budget);
            draw->AddText(ImVec2(origin.x+4,y+2),IM_COL32_WHITE,label);
        }
        if (ImGui::IsItemHovered()) {
            const size_t hovered=std::min(frames.size()-1,static_cast<size_t>(std::max(0.f,ImGui::GetIO().MousePos.x-origin.x)/barWidth));
            const auto& frame=frames[hovered];
            ImGui::BeginTooltip(); ImGui::Text("Frame %llu  |  %.3f ms",static_cast<unsigned long long>(frame.id),frame.totalMs);
            for (size_t c=0;c<CategoryCount;++c) ImGui::Text("%s: %.3f ms",CategoryName(static_cast<Category>(c)),frame.categories[c]);
            ImGui::EndTooltip();
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) selected=frame.id;
        }
        for (size_t c=0;c<CategoryCount;++c) {
            if (c && ImGui::GetItemRectMax().x+ImGui::CalcTextSize(CategoryName(static_cast<Category>(c))).x+40<origin.x+width) ImGui::SameLine();
            ImGui::ColorButton(CategoryName(static_cast<Category>(c)),ImGui::ColorConvertU32ToFloat4(colors[c]),ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoDragDrop,ImVec2(9,9));
            ImGui::SameLine(); ImGui::TextUnformatted(CategoryName(static_cast<Category>(c)));
        }
        const auto& frame=selected ? *std::find_if(frames.begin(),frames.end(),[&](const Frame& f){return f.id==selected;}) : frames.back();
        ImGui::Separator();
        ImGui::TextWrapped("CPU / Main Thread   |   Frame %llu   |   %.3f ms   |   %zu samples",
            static_cast<unsigned long long>(frame.id),frame.totalMs,frame.samples.size());
        if (frame.dropped) ImGui::TextDisabled("Truncated capture: %zu samples omitted",frame.dropped);
        if (ImGui::BeginTabBar("Profiler views")) {
            if (ImGui::BeginTabItem("Hierarchy")) {
                ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##Search","Search samples",&search);
                std::vector<Row> rows;
                std::map<std::pair<int,std::string>,int> lookup;
                std::vector<int> mapping(frame.samples.size());
                for (size_t i=0;i<frame.samples.size();++i) {
                    const auto& sample=frame.samples[i];
                    int parent=sample.parent<0 ? -1 : mapping[sample.parent];
                    auto key=std::make_pair(parent,sample.name);
                    auto found=lookup.find(key);
                    int row;
                    if (found==lookup.end()) { row=static_cast<int>(rows.size()); lookup[key]=row; rows.push_back({sample.name,sample.category,parent}); }
                    else row=found->second;
                    mapping[i]=row; rows[row].total+=sample.totalMs; rows[row].self+=sample.selfMs; ++rows[row].calls;
                }
                std::vector<std::vector<int>> children(rows.size()+1);
                for (int i=0;i<static_cast<int>(rows.size());++i) children[rows[i].parent+1].push_back(i);
                for (auto& list:children) std::stable_sort(list.begin(),list.end(),[&](int a,int b){return rows[a].total>rows[b].total;});
                std::vector<bool> visible(rows.size(),search.empty());
                for (int i=static_cast<int>(rows.size())-1;i>=0;--i) {
                    visible[i]=visible[i] || rows[i].name.find(search)!=std::string::npos;
                    if (visible[i] && rows[i].parent>=0) visible[rows[i].parent]=true;
                }
                if (ImGui::BeginTable("CPU hierarchy",5,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY|ImGuiTableFlags_ScrollX,
                    ImVec2(0,std::max(80.f,ImGui::GetContentRegionAvail().y)),std::max(600.f,ImGui::GetContentRegionAvail().x))) {
                    ImGui::TableSetupColumn("Sample",ImGuiTableColumnFlags_WidthStretch,3);
                    for (const char* name : {"Total ms","Self ms","Calls","Frame %"}) ImGui::TableSetupColumn(name);
                    ImGui::TableSetupScrollFreeze(1,1); ImGui::TableHeadersRow();
                    std::function<void(int)> show=[&](int parent) {
                        for (int id:children[parent+1]) {
                            if (!visible[id]) continue;
                            const auto& row=rows[id]; ImGui::PushID(id);
                            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                            auto flags=ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_OpenOnArrow;
                            if (row.parent < 0) flags|=ImGuiTreeNodeFlags_DefaultOpen;
                            if (children[id+1].empty()) flags|=ImGuiTreeNodeFlags_Leaf;
                            if (!search.empty()) ImGui::SetNextItemOpen(true);
                            const bool open=ImGui::TreeNodeEx("##sample",flags,"%s",row.name.c_str());
                            Tip(row.name.c_str());
                            ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f",row.total);
                            ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f",row.self);
                            ImGui::TableSetColumnIndex(3); ImGui::Text("%d",row.calls);
                            ImGui::TableSetColumnIndex(4); ImGui::Text("%.1f",frame.totalMs>0 ? 100*row.total/frame.totalMs : 0);
                            if (open) { show(id); ImGui::TreePop(); } ImGui::PopID();
                        }
                    }; show(-1); ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Timeline")) {
                ImGui::SetNextItemWidth(180); ImGui::SliderFloat("Zoom",&zoom,1,20,"%.1fx");
                ImGui::BeginChild("CPU timeline",ImVec2(0,0),ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
                const auto p=ImGui::GetCursorScreenPos(); const float w=std::max(1.f,ImGui::GetContentRegionAvail().x)*zoom;
                int depth=0; for (const auto& s:frame.samples) depth=std::max(depth,s.depth);
                ImGui::Dummy(ImVec2(w,(depth+2)*24.f)); draw=ImGui::GetWindowDrawList();
                for (const auto& sample:frame.samples) {
                    float x=p.x+static_cast<float>(sample.startMs/std::max(.001,frame.totalMs))*w;
                    float sw=std::max(1.f,static_cast<float>(sample.totalMs/std::max(.001,frame.totalMs))*w);
                    ImVec2 a(x,p.y+sample.depth*24),b(x+sw,p.y+sample.depth*24+21);
                    draw->AddRectFilled(a,b,colors[static_cast<size_t>(sample.category)],2);
                    if (sw>50) { draw->PushClipRect(a,b,true); draw->AddText(ImVec2(a.x+3,a.y+2),IM_COL32(15,15,18,255),sample.name.c_str()); draw->PopClipRect(); }
                    if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(a,b)) ImGui::SetTooltip("%s\nStart %.3f ms\nTotal %.3f ms / Self %.3f ms",sample.name.c_str(),sample.startMs,sample.totalMs,sample.selfMs);
                }
                ImGui::EndChild(); ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
}
