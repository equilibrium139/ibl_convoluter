int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: mesh_converter model1_path [model2_path] ... [modeln_path]\n";
    }

    for (int i = 1; i < argc; i++)
    {
        ConvertMesh(argv[i]);
    }

    return 0;
}